/* ============================================================================
*  testeC.c  -  Bateria de testes do coprocessador gráfico (DE1-SoC)
*
*  Compilar (C + driver assembly juntos):
*      gcc -O2 -Wall testeC.c assembly.S -o teste -lrt
*  Executar (precisa de root por causa do /dev/mem):
*      sudo ./teste                 -> avança com ENTER (q+ENTER sai)
*      sudo ./teste -t 2000         -> avança sozinho a cada 2000 ms
*      sudo ./teste -c 5            -> roda só a cena 5
*      sudo ./teste -l              -> lista as cenas
*
*  Resolução lógica: 320x240 (o VGA mostra 640x480, cada pixel lógico = 2x2).
*  Cor: 8 bits RGB332 (RRRGGGBB).  Cor 0 = TRANSPARENTE em sprites/polígonos.
*
*  ISA (opcode nos bits [4:0], operandos nos bits [31:5]):
*    0 NoOp     espera o próximo VSYNC (usamos como "fim de quadro")
*    1 BSCROLL  X[15:10] Y[9:5]                       (X<=39, Y<=29: em tiles)
*    2 BTILE    ENDERECO[23:13] COR[12:5]             (endereço = linha*40+coluna)
*    3 POSSP    ID[26:22] X[21:13] Y[12:5]
*    4 ATSP     ID[12:8] FLIP_V[7] FLIP_H[6] SHOW[5]
*    5 WRSP     ENDERECO[25:13] COR[12:5]             (end = slot*256 + y*16 + x)
*    6 RGSP     ID[14:10] SLOT_RAM[9:5]
*    7 ATPO     ID[12:9] SHOW[8] TIPO[7:5]
*    8 POSPO    ID[25:22] X[21:13] Y[12:5]
*    9 TAMPO    ID[25:22] LARGURA[21:13] ALTURA[12:5]
*   10 CORPO    ID[16:13] COR[12:5]
*   11 PRIOR    PRI[5]    (0 = sprite sobre polígono, 1 = polígono sobre sprite)
*
*  Tipos de polígono: 0 retângulo, 1 quadrado (lado = largura), 2 triângulo
*  equilátero (X = vértice do topo, base ±L/2), 3 isósceles (X = topo, base ±L/4),
*  4 escaleno/retângulo (vértice reto embaixo à esquerda).
*  Menor ID = maior prioridade (sprite 0 e polígono 0 ficam por cima).
* ========================================================================== */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <signal.h>


/* ---- driver assembly (assembly.S) ---------------------------------------- */
extern int  gpu_init(void);                    /* 0 ok, -1 erro           */
extern int  enviar_instrucao(uint32_t instr);  /* 0 ok, -1 erro/timeout   */
extern void gpu_close(void);


#ifdef SIM_HOOK                                /* só usado no simulador de bancada */
extern void sim_snapshot(const char *titulo);
#endif


/* ---- limites do hardware -------------------------------------------------- */
#define LARGURA        320
#define ALTURA         240
#define TILES_X        40
#define TILES_Y        30
#define N_TILES        (TILES_X * TILES_Y)     /* 1200 */
#define N_SPRITES      32
#define N_SPRITE_RAM   32                      /* 32 slots de 16x16 = 8192 bytes */
#define SPR_TAM        16
#define N_POLIGONOS    16


/* ---- opcodes -------------------------------------------------------------- */
enum { OP_NOOP, OP_BSCROLL, OP_BTILE, OP_POSSP, OP_ATSP, OP_WRSP, OP_RGSP,
      OP_ATPO, OP_POSPO, OP_TAMPO, OP_CORPO, OP_PRIOR };


/* ---- paleta RGB332 -------------------------------------------------------- */
#define COR_PRETO     0x00
#define COR_AZUL      0x03
#define COR_VERDE     0x1C
#define COR_CIANO     0x1F
#define COR_VERMELHO  0xE0
#define COR_MAGENTA   0xE3
#define COR_LARANJA   0xF0
#define COR_AMARELO   0xFC
#define COR_BRANCO    0xFF
#define COR_CINZA     0x92
#define COR_ROXO      0x62


/* ============================================================================
*  Codificadores de instrução (ISA nova, opcode em [4:0])
* ========================================================================== */
static inline uint32_t i_noop(void) { return OP_NOOP; }
static inline uint32_t i_bscroll(unsigned x, unsigned y)
{ return OP_BSCROLL | ((x & 0x3Fu) << 10) | ((y & 0x1Fu) << 5); }
static inline uint32_t i_btile(unsigned end, unsigned cor)
{ return OP_BTILE | ((end & 0x7FFu) << 13) | ((cor & 0xFFu) << 5); }
static inline uint32_t i_possp(unsigned id, unsigned x, unsigned y)
{ return OP_POSSP | ((id & 0x1Fu) << 22) | ((x & 0x1FFu) << 13) | ((y & 0xFFu) << 5); }
static inline uint32_t i_atsp(unsigned id, unsigned flipv, unsigned fliph, unsigned show)
{ return OP_ATSP | ((id & 0x1Fu) << 8) | ((flipv & 1u) << 7) | ((fliph & 1u) << 6) | ((show & 1u) << 5); }
static inline uint32_t i_wrsp(unsigned end, unsigned cor)
{ return OP_WRSP | ((end & 0x1FFFu) << 13) | ((cor & 0xFFu) << 5); }
static inline uint32_t i_rgsp(unsigned id, unsigned slot)
{ return OP_RGSP | ((id & 0x1Fu) << 10) | ((slot & 0x1Fu) << 5); }
static inline uint32_t i_atpo(unsigned id, unsigned tipo, unsigned show)
{ return OP_ATPO | ((id & 0xFu) << 9) | ((show & 1u) << 8) | ((tipo & 7u) << 5); }
static inline uint32_t i_pospo(unsigned id, unsigned x, unsigned y)
{ return OP_POSPO | ((id & 0xFu) << 22) | ((x & 0x1FFu) << 13) | ((y & 0xFFu) << 5); }
static inline uint32_t i_tampo(unsigned id, unsigned larg, unsigned alt)
{ return OP_TAMPO | ((id & 0xFu) << 22) | ((larg & 0x1FFu) << 13) | ((alt & 0xFFu) << 5); }
static inline uint32_t i_corpo(unsigned id, unsigned cor)
{ return OP_CORPO | ((id & 0xFu) << 13) | ((cor & 0xFFu) << 5); }
static inline uint32_t i_prior(unsigned p)
{ return OP_PRIOR | ((p & 1u) << 5); }


/* ============================================================================
*  Infraestrutura: envio, tempo, interação com o terminal
* ========================================================================== */
static int           g_tempo_ms = -1;          /* -1 = espera ENTER; >=0 = timer */
static unsigned long g_enviadas = 0;
static volatile sig_atomic_t g_sair = 0;


static double agora_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}


static void dormir_ms(int ms)
{
   struct timespec ts;
   ts.tv_sec  = ms / 1000;
   ts.tv_nsec = (long)(ms % 1000) * 1000000L;
   nanosleep(&ts, NULL);
}


static void on_sigint(int s) { (void)s; g_sair = 1; }


static void limpar_tela(void);


static void sair_limpo(void)
{
   limpar_tela();
   gpu_close();
   printf("\nEncerrado. %lu instruções enviadas.\n", g_enviadas);
   exit(0);
}


/* envia 1 instrução; se o handshake estourar o timeout, aborta com diagnóstico */
static void enviar(uint32_t instr)
{
   if (enviar_instrucao(instr) != 0) {
       fprintf(stderr,
           "\nERRO: sem 'done' do coprocessador (instrução #%lu = 0x%08X).\n"
           "      O bitstream (.sof) está carregado? O reset (KEY0) está solto?\n",
           g_enviadas, instr);
       gpu_close();
       exit(1);
   }
   g_enviadas++;
}


/* fim de quadro: o NoOp faz a UC esperar o VSYNC (~16,7 ms a 60 Hz) */
static void frame(void)
{
   if (g_sair) sair_limpo();
   enviar(i_noop());
}


static void frames(int n) { while (n-- > 0) frame(); }


/* descreve o que deve aparecer e espera ENTER ou o timer */
static void esperar(const char *titulo, const char *esperado)
{
#ifdef SIM_HOOK
   sim_snapshot(titulo);
#endif
   printf("\n  [%s]\n      esperado: %s\n", titulo, esperado);
   if (g_tempo_ms >= 0) {
       printf("      (avanca em %d ms)\n", g_tempo_ms);
       fflush(stdout);
       dormir_ms(g_tempo_ms);
   } else {
       char buf[16];
       printf("      ENTER = continuar | q+ENTER = sair > ");
       fflush(stdout);
       if (!fgets(buf, sizeof buf, stdin) || buf[0] == 'q' || buf[0] == 'Q')
           g_sair = 1;
   }
   if (g_sair) sair_limpo();
}


/* ---- utilidades numéricas (sem libm: o gcc do lab não usa -lm) ------------ */
static const int8_t SEN[64] = {
     0,  12,  25,  37,  49,  60,  71,  81,  90,  98, 106, 112, 117, 122, 125, 126,
   127, 126, 125, 122, 117, 112, 106,  98,  90,  81,  71,  60,  49,  37,  25,  12,
     0, -12, -25, -37, -49, -60, -71, -81, -90, -98,-106,-112,-117,-122,-125,-126,
  -127,-126,-125,-122,-117,-112,-106, -98, -90, -81, -71, -60, -49, -37, -25, -12 };
static int isen(int i) { return SEN[i & 63]; }
static int icos(int i) { return SEN[(i + 16) & 63]; }


static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }


/* onda triangular 0..max..0 (para "quicar" sem if's) */
static int tri(int t, int max)
{
   int p;
   if (max <= 0) return 0;
   p = t % (2 * max);
   if (p < 0) p += 2 * max;
   return p <= max ? p : 2 * max - p;
}


static uint32_t g_rnd = 12345u;
static uint32_t rnd(void) { g_rnd = g_rnd * 1664525u + 1013904223u; return g_rnd >> 8; }


/* ============================================================================
*  Funções de alto nível sobre a ISA
* ========================================================================== */


/* ---- background ----------------------------------------------------------- */
static void bg_tile(int tx, int ty, unsigned cor) { enviar(i_btile(ty * TILES_X + tx, cor)); }


static void bg_preencher(unsigned cor)
{
   int i;
   for (i = 0; i < N_TILES; i++) enviar(i_btile(i, cor));
}


static void bg_xadrez(unsigned c1, unsigned c2, int lado)
{
   int tx, ty;
   for (ty = 0; ty < TILES_Y; ty++)
       for (tx = 0; tx < TILES_X; tx++)
           bg_tile(tx, ty, (((tx / lado) + (ty / lado)) & 1) ? c1 : c2);
}


static void bg_gradiente(void)       /* R cresce em X, G cresce em Y */
{
   int tx, ty;
   for (ty = 0; ty < TILES_Y; ty++)
       for (tx = 0; tx < TILES_X; tx++)
           bg_tile(tx, ty, (((tx * 8 / TILES_X) << 5) | ((ty * 8 / TILES_Y) << 2) | 0x01));
}


static void bg_moldura(unsigned cor)
{
   int i;
   for (i = 0; i < TILES_X; i++) { bg_tile(i, 0, cor); bg_tile(i, TILES_Y - 1, cor); }
   for (i = 0; i < TILES_Y; i++) { bg_tile(0, i, cor); bg_tile(TILES_X - 1, i, cor); }
}


/* ---- sprites -------------------------------------------------------------- */
/* padrões 16x16 em ASCII: '.' transparente, letra = cor */
static const char *PAD_F[16] = {      /* "F" assimétrico: serve para ver os flips */
   "WWWWWWWWWW......", "WWWWWWWWWW......", "WW..............", "WW..............",
   "WW..............", "WWWWWWW.........", "WWWWWWW.........", "WW..............",
   "WW..............", "WW..............", "WW..............", "WW..............",
   "WW..............", "WW..............", "WW............RR", "WW............RR" };


static const char *PAD_CARINHA[16] = {
   "....YYYYYYYY....", "..YYYYYYYYYYYY..", ".YYYYYYYYYYYYYY.", ".YYYBBYYYYBBYYY.",
   "YYYYBBYYYYBBYYYY", "YYYYYYYYYYYYYYYY", "YYYYYYYYYYYYYYYY", "YYYYYYYYYYYYYYYY",
   "YYRYYYYYYYYYYRYY", "YYYRYYYYYYYYRYYY", ".YYYRRRRRRRRYYY.", ".YYYYYRRRRYYYYY.",
   ".YYYYYYYYYYYYYY.", "..YYYYYYYYYYYY..", "....YYYYYYYY....", "................" };


static unsigned letra_cor(char c)
{
   switch (c) {
   case 'W': return COR_BRANCO;   case 'R': return COR_VERMELHO;
   case 'G': return COR_VERDE;    case 'B': return COR_AZUL;
   case 'Y': return COR_AMARELO;  case 'C': return COR_CIANO;
   case 'M': return COR_MAGENTA;  case 'O': return COR_LARANJA;
   default:  return COR_PRETO;
   }
}


/* padrões: 0=F  1=carinha  2=xadrez com furos  3=moldura com diagonais */
#define N_PADROES 4
static unsigned sprite_pixel(int padrao, int x, int y)
{
   switch (padrao) {
   case 0:  return letra_cor(PAD_F[y][x]);
   case 1:  return letra_cor(PAD_CARINHA[y][x]);
   case 2:  return (((x / 4) + (y / 4)) & 1) ? COR_CIANO : COR_PRETO;
   default:
       if (x == 0 || y == 0 || x == 15 || y == 15) return COR_VERDE;
       if (x == y || x + y == 15)                  return COR_AMARELO;
       return COR_PRETO;
   }
}


/* grava um padrão (256 WRSP) no slot da RAM de sprites */
static void sprite_carregar(int slot, int padrao)
{
   int x, y;
   for (y = 0; y < SPR_TAM; y++)
       for (x = 0; x < SPR_TAM; x++)
           enviar(i_wrsp(slot * 256 + y * SPR_TAM + x, sprite_pixel(padrao, x, y)));
}


/* sprite de slot "slot" com cor derivada do número (teste de RAM cheia) */
static unsigned slot_pixel(int slot, int x, int y)
{
   unsigned base = 0x20 + slot * 7;                    /* nunca é 0 */
   if (x == 0 || y == 0 || x == 15 || y == 15) return COR_BRANCO;
   if (x == y) return base ^ 0x24;
   return base;
}


static void sprite_carregar_slot_cheio(int slot)
{
   int x, y;
   for (y = 0; y < SPR_TAM; y++)
       for (x = 0; x < SPR_TAM; x++)
           enviar(i_wrsp(slot * 256 + y * SPR_TAM + x, slot_pixel(slot, x, y)));
}


static void sprite_pos(int id, int x, int y)
{
   enviar(i_possp(id, clampi(x, 0, LARGURA - 1), clampi(y, 0, ALTURA - 1)));
}


/* configura um sprite completo: slot da RAM, posição, flips e visibilidade */
static void sprite_cfg(int id, int slot, int x, int y, int flipv, int fliph, int show)
{
   enviar(i_rgsp(id, slot));
   sprite_pos(id, x, y);
   enviar(i_atsp(id, flipv, fliph, show));
}


/* ---- polígonos ------------------------------------------------------------ */
/* (x,y) = âncora do hardware (vértice A). Tipos 2 e 3: x = vértice do topo */
static void poligono(int id, int tipo, int x, int y, int larg, int alt, unsigned cor, int show)
{
   enviar(i_atpo(id, tipo, show));
   enviar(i_pospo(id, clampi(x, 0, LARGURA - 1), clampi(y, 0, ALTURA - 1)));
   enviar(i_tampo(id, larg, alt));
   enviar(i_corpo(id, cor));
}


/* posiciona pelo CENTRO da caixa envolvente (calcula a âncora de cada tipo) */
static void poligono_centro(int id, int tipo, int cx, int cy, int larg, int alt, unsigned cor)
{
   int h = (tipo == 1) ? larg : alt;                   /* quadrado: altura = largura */
   int x = (tipo == 2 || tipo == 3) ? cx : cx - larg / 2;
   poligono(id, tipo, x, cy - h / 2, larg, alt, cor, 1);
}


static void poligono_mover_centro(int id, int tipo, int cx, int cy, int larg, int alt)
{
   int h = (tipo == 1) ? larg : alt;
   int x = (tipo == 2 || tipo == 3) ? cx : cx - larg / 2;
   enviar(i_pospo(id, clampi(x, 0, LARGURA - 1), clampi(cy - h / 2, 0, ALTURA - 1)));
}


/* ---- limpar tudo ---------------------------------------------------------- */
static void limpar_tela(void)
{
   int i;
   for (i = 0; i < N_SPRITES; i++)   enviar(i_atsp(i, 0, 0, 0));
   for (i = 0; i < N_POLIGONOS; i++) enviar(i_atpo(i, 0, 0));
   enviar(i_prior(0));
   enviar(i_bscroll(0, 0));
   bg_preencher(COR_PRETO);
   frames(2);
}


/* ============================================================================
*  CENAS
* ========================================================================== */


/* ---- 1: NoOp / VSYNC / opcodes inválidos ---------------------------------- */
static void cena_noop(void)
{
   double t0, dt;
   const int N = 120;
   int i;


   limpar_tela();
   bg_gradiente();


   esperar("NoOp 1/3 - fundo de referência",
           "gradiente (vermelho cresce p/ direita, verde p/ baixo)");


   printf("      enviando %d NoOps (1 por quadro)...\n", N);
   t0 = agora_ms();
   for (i = 0; i < N; i++) frame();
   dt = agora_ms() - t0;
   printf("      %d NoOps em %.1f ms -> %.2f ms/quadro = %.2f fps (esperado ~16,7 ms / 60 fps)\n",
          N, dt, dt / N, 1000.0 * N / dt);


   esperar("NoOp 2/3 - opcodes inválidos",
           "imagem IDÊNTICA: opcodes 12, 13, 20 e 31 devem ser ignorados");
   enviar(12u | (0x1234567u << 5));
   enviar(13u | (0x7FFFFFFu << 5));
   enviar(20u | (0x0F0F0F0u << 5));
   enviar(31u | (0x7FFFFFFu << 5));
   frames(3);


   esperar("NoOp 3/3 - NoOps em rajada",
           "imagem continua igual; 40 NoOps seguidos (FIFO enche e a UC consome 1/quadro)");
   t0 = agora_ms();
   for (i = 0; i < 40; i++) frame();
   dt = agora_ms() - t0;
   printf("      40 NoOps em %.1f ms (%.2f ms/quadro)\n", dt, dt / 40);
}


/* ---- 2: BTILE -------------------------------------------------------------- */
static void cena_bg_tiles(void)
{
   int tx, ty;


   limpar_tela();


   bg_xadrez(COR_BRANCO, COR_AZUL, 1);
   frames(2);
   esperar("BTILE 1/4 - xadrez 1x1",
           "xadrez branco/azul de 40x30 tiles (cada tile 8x8 px)");


   bg_xadrez(COR_VERMELHO, COR_AMARELO, 5);
   frames(2);
   esperar("BTILE 2/4 - xadrez 5x5",
           "blocos de 5x5 tiles vermelho/amarelo");


   bg_gradiente();
   frames(2);
   esperar("BTILE 3/4 - gradiente",
           "R cresce da esquerda p/ direita, G de cima p/ baixo (todos os 1200 tiles)");


   bg_preencher(COR_PRETO);
   bg_moldura(COR_BRANCO);
   bg_tile(0, 0, COR_VERMELHO);             bg_tile(TILES_X - 1, 0, COR_VERDE);
   bg_tile(0, TILES_Y - 1, COR_AZUL);       bg_tile(TILES_X - 1, TILES_Y - 1, COR_AMARELO);
   for (tx = 0; tx < TILES_X; tx++) bg_tile(tx, TILES_Y / 2, COR_CIANO);
   for (ty = 0; ty < TILES_Y; ty++) bg_tile(TILES_X / 2, ty, COR_MAGENTA);
   frames(2);
   esperar("BTILE 4/4 - endereçamento",
           "moldura branca; cantos: sup-esq VERMELHO, sup-dir VERDE, inf-esq AZUL, inf-dir AMARELO; cruz ciano/magenta");
}


/* ---- 3: BSCROLL ------------------------------------------------------------ */
static void cena_bg_scroll(void)
{
   int i;


   limpar_tela();
   bg_xadrez(COR_BRANCO, COR_AZUL, 4);
   bg_moldura(COR_VERMELHO);
   bg_tile(0, 0, COR_AMARELO);
   frames(2);
   esperar("BSCROLL 1/4 - referência", "xadrez 4x4 com moldura vermelha e tile amarelo no canto sup-esq");


   for (i = 0; i < TILES_X; i++) { enviar(i_bscroll(i, 0)); frames(3); }
   esperar("BSCROLL 2/4 - horizontal 0..39",
           "o fundo andou 1 tile por 3 quadros p/ a ESQUERDA e fechou o ciclo (wrap)");


   for (i = 0; i < TILES_Y; i++) { enviar(i_bscroll(0, i)); frames(3); }
   esperar("BSCROLL 3/4 - vertical 0..29",
           "o fundo andou p/ CIMA 1 tile por 3 quadros e fechou o ciclo (wrap)");


   for (i = 0; i < 120; i++) { enviar(i_bscroll(i % TILES_X, (i * 2) % TILES_Y)); frames(1); }
   esperar("BSCROLL 4/4 - diagonal rápida",
           "movimento diagonal contínuo (120 quadros, 1 BSCROLL por quadro)");


   enviar(i_bscroll(TILES_X - 1, TILES_Y - 1));
   frames(2);
   esperar("BSCROLL - extremo (39,29)",
           "fundo deslocado no máximo: 1 tile antes do ciclo completo");
   enviar(i_bscroll(0, 0));
   frames(2);
}


/* ---- 4: sprites individuais ------------------------------------------------ */
static void cena_sprite_individual(void)
{
   int i, t;


   limpar_tela();
   bg_xadrez(COR_CINZA, COR_ROXO, 2);
   frames(2);


   sprite_carregar(0, 0);                              /* slot 0 = "F"      */
   sprite_carregar(1, 1);                              /* slot 1 = carinha  */
   sprite_carregar(2, 2);                              /* slot 2 = xadrez   */
   sprite_carregar(3, 3);                              /* slot 3 = moldura  */
   frames(1);
   printf("      (4 slots carregados: 4 x 256 WRSP)\n");


   sprite_cfg(0, 0, 152, 112, 0, 0, 1);
   frames(2);
   esperar("SPRITE 1/6 - POSSP+ATSP+RGSP+WRSP",
           "UM sprite 'F' branco (canto inferior direito vermelho) no centro da tela");


   sprite_cfg(0, 0,  60, 100, 0, 0, 1);
   sprite_cfg(1, 0, 100, 100, 0, 1, 1);
   sprite_cfg(2, 0, 140, 100, 1, 0, 1);
   sprite_cfg(3, 0, 180, 100, 1, 1, 1);
   frames(2);
   esperar("SPRITE 2/6 - flips",
           "4 'F': normal | espelhado H | espelhado V | H+V (marca vermelha muda de canto)");


   sprite_cfg(0, 0,   0,   0, 0, 0, 1);
   sprite_cfg(1, 1, 304,   0, 0, 0, 1);
   sprite_cfg(2, 2,   0, 224, 0, 0, 1);
   sprite_cfg(3, 3, 304, 224, 0, 0, 1);
   enviar(i_atsp(4, 0, 0, 0));
   frames(2);
   esperar("SPRITE 3/6 - cantos da tela",
           "4 sprites nos 4 cantos: F (sup-esq), carinha (sup-dir), xadrez ciano com furos (inf-esq), moldura verde (inf-dir)");


   sprite_cfg(0, 1, 144, 104, 0, 0, 1);
   sprite_cfg(1, 2, 150, 110, 0, 0, 1);
   sprite_cfg(2, 3, 156, 116, 0, 0, 1);
   sprite_cfg(3, 0, 162, 122, 0, 0, 1);
   frames(2);
   esperar("SPRITE 4/6 - sobreposição e transparência",
           "pilha diagonal: carinha por cima de tudo, depois xadrez (fundo aparece nos furos), moldura, F (sprite 0 = prioridade máxima)");


   printf("      movendo sprite 0 pelas bordas (300 quadros)...\n");
   sprite_cfg(0, 1, 0, 0, 0, 0, 1);
   sprite_cfg(1, 2, 8, 8, 0, 0, 0);
   enviar(i_atsp(2, 0, 0, 0));
   enviar(i_atsp(3, 0, 0, 0));
   for (t = 0; t < 300; t++) {
       sprite_pos(0, tri(t * 3, LARGURA - SPR_TAM), tri(t * 2, ALTURA - SPR_TAM));
       frame();
   }
   esperar("SPRITE 5/6 - movimento", "a carinha quicou pela tela inteira (sem rastro nem deformar)");


   for (i = 0; i < 8; i++) {
       enviar(i_atsp(0, 0, 0, i & 1));
       frames(30);
   }
   esperar("SPRITE 6/6 - show/hide", "carinha piscou 4 vezes (ATSP show=1/0); terminou escondida (i=7 -> show=1: visível)");
}


/* ---- 5: 32 sprites ao mesmo tempo ------------------------------------------ */
static void cena_sprites_max(void)
{
   int i, t;
   int px[N_SPRITES], py[N_SPRITES];


   limpar_tela();
   bg_xadrez(COR_AZUL, COR_ROXO, 3);
   for (i = 0; i < N_PADROES; i++) sprite_carregar(i, i);


   for (i = 0; i < N_SPRITES; i++) {
       px[i] = 20 + (i % 8) * 36;
       py[i] = 30 + (i / 8) * 48;
       sprite_cfg(i, i % N_PADROES, px[i], py[i], (i / 8) & 1, (i / 16) & 1, 1);
   }
   frames(2);
   esperar("32 SPRITES 1/3 - grade 8x4",
           "32 sprites (todos os IDs 0..31) em grade 8x4, 4 padrões repetidos, com flips variados");


   printf("      animando os 32 sprites (400 quadros)...\n");
   for (t = 0; t < 400; t++) {
       for (i = 0; i < N_SPRITES; i++)
           sprite_pos(i, tri(px[i] + t * (1 + i % 4), LARGURA - SPR_TAM),
                         tri(py[i] + t * (1 + i % 3), ALTURA - SPR_TAM));
       frame();
   }
   esperar("32 SPRITES 2/3 - movimento",
           "32 sprites quicando em velocidades diferentes, sem piscar nem perder nenhum");


   for (i = 0; i < N_SPRITES; i++) sprite_pos(i, 100 + i * 4, 60 + i * 3);
   frames(2);
   esperar("32 SPRITES 3/3 - prioridade entre sprites",
           "cascata diagonal: o sprite 0 (menor ID) fica por cima, o 31 por baixo");
}


/* ---- 6: RAM de sprites cheia ----------------------------------------------- */
static void cena_sprite_ram(void)
{
   int s;
   double t0, dt;


   limpar_tela();
   bg_preencher(COR_PRETO);


   printf("      gravando 32 slots x 256 pixels = 8192 WRSP...\n");
   t0 = agora_ms();
   for (s = 0; s < N_SPRITE_RAM; s++) sprite_carregar_slot_cheio(s);
   dt = agora_ms() - t0;
   printf("      8192 instruções em %.1f ms -> %.2f us/instrução (%.0f instr/s)\n",
          dt, dt * 1000.0 / 8192.0, 8192.0 / (dt / 1000.0));


   for (s = 0; s < N_SPRITE_RAM; s++)
       sprite_cfg(s, s, 20 + (s % 8) * 36, 30 + (s / 8) * 48, 0, 0, 1);
   frames(2);
   esperar("RAM SPRITES - 32 slots",
           "32 quadrados com borda branca e uma diagonal, cada um de cor diferente (slot 0 = escuro ... slot 31)");


   for (s = 0; s < N_SPRITE_RAM; s++) enviar(i_rgsp(s, 31 - s));
   frames(2);
   esperar("RAM SPRITES - RGSP trocando slots",
           "as MESMAS 32 posições, agora com as cores em ordem invertida (RGSP religou id -> slot 31-id)");
}


/* ---- 7: polígonos individuais ---------------------------------------------- */
static void cena_poligonos_ind(void)
{
   static const char *NOME[5] = { "retângulo", "quadrado", "triângulo equilátero",
                                  "triângulo isósceles", "triângulo escaleno (retângulo)" };
   static const unsigned CORES[8] = { COR_VERMELHO, COR_LARANJA, COR_AMARELO, COR_VERDE,
                                      COR_CIANO, COR_AZUL, COR_MAGENTA, COR_BRANCO };
   int tipo, i, t;
   char msg[96];


   limpar_tela();
   bg_xadrez(COR_CINZA, COR_ROXO, 4);
   frames(2);


   for (tipo = 0; tipo < 5; tipo++) {
       poligono_centro(0, tipo, 160, 120, 120, 100, COR_AMARELO);
       frames(2);
       snprintf(msg, sizeof msg, "POLIGONO 1/5 - tipo %d", tipo);
       esperar(msg, NOME[tipo]);
   }


   poligono_centro(0, 0, 160, 120, 120, 100, COR_VERMELHO);
   for (i = 0; i < 8; i++) { enviar(i_corpo(0, CORES[i])); frames(30); }
   esperar("POLIGONO 2/5 - CORPO", "retângulo mudou de cor 8 vezes (vermelho..branco)");


   enviar(i_corpo(0, COR_PRETO));
   frames(2);
   esperar("POLIGONO 3/5 - cor 0", "retângulo SUMIU (cor 0 = transparente) - o xadrez aparece no lugar");
   enviar(i_corpo(0, COR_VERDE));


   for (t = 8; t <= 200; t += 2) { enviar(i_tampo(0, t * 3 / 2, t / 2 + 20)); frame(); }
   for (t = 200; t >= 8; t -= 2) { enviar(i_tampo(0, t * 3 / 2, t / 2 + 20)); frame(); }
   esperar("POLIGONO 4/5 - TAMPO", "o retângulo verde cresceu e encolheu (ancorado no canto sup-esq)");


   enviar(i_tampo(0, 60, 40));
   for (t = 0; t < 300; t++) {
       enviar(i_pospo(0, tri(t * 2, LARGURA - 60), tri(t * 2, ALTURA - 40)));
       frame();
   }
   enviar(i_atpo(0, 0, 0));
   frames(2);
   esperar("POLIGONO 5/5 - POSPO e hide",
           "retângulo percorreu a tela e depois ESCONDEU (ATPO show=0): tela só com o xadrez");
}


/* ---- 8: 16 polígonos ao mesmo tempo ---------------------------------------- */
static void cena_poligonos_max(void)
{
   static const unsigned COR[16] = {
       COR_VERMELHO, COR_VERDE, COR_AZUL, COR_AMARELO, COR_CIANO, COR_MAGENTA,
       COR_LARANJA, COR_BRANCO, 0xE8, 0x3C, 0x6F, 0xB4, 0x5B, 0xD2, 0x27, 0x8D };
   int i, t;


   limpar_tela();
   bg_preencher(COR_PRETO);


   for (i = 0; i < N_POLIGONOS; i++)
       poligono_centro(i, i % 5, 40 + (i % 4) * 80, 30 + (i / 4) * 60, 56, 44, COR[i]);
   frames(2);
   esperar("16 POLIGONOS 1/3 - grade 4x4",
           "16 formas (IDs 0..15), tipos 0-4 repetidos: retângulo, quadrado, equilátero, isósceles, escaleno");


   printf("      animando 16 polígonos (400 quadros)...\n");
   for (t = 0; t < 400; t++) {
       for (i = 0; i < N_POLIGONOS; i++)
           poligono_mover_centro(i, i % 5,
               160 + isen(t + i * 4) * 110 / 127 * (1 + (i & 1)) / 2 + icos(i * 4) * 40 / 127,
               120 + icos(t + i * 4) * 80 / 127, 56, 44);
       frame();
   }
   esperar("16 POLIGONOS 2/3 - movimento", "16 formas orbitando em fases diferentes, todas visíveis");


   for (i = 0; i < N_POLIGONOS; i++) poligono_centro(i, 0, 160 - i * 4, 120 - i * 3, 150 - i * 6, 120 - i * 5, COR[i]);
   frames(2);
   esperar("16 POLIGONOS 3/3 - prioridade",
           "retângulos concêntricos: o ID 0 (vermelho) é o MAIOR e fica por cima? NÃO - todos têm centro/tam. decrescentes; "
           "o vermelho (ID 0) está por cima de todos os outros nas áreas de sobreposição");
}


/* ---- 9: PRIOR (sprite x polígono) ------------------------------------------ */
static void cena_prioridade(void)
{
   int i;


   limpar_tela();
   bg_gradiente();
   sprite_carregar(0, 1);
   sprite_cfg(0, 0, 150, 110, 0, 0, 1);
   sprite_cfg(1, 0, 130, 100, 0, 0, 1);
   sprite_cfg(2, 0, 170, 120, 0, 0, 1);
   poligono(0, 0, 100, 90, 120, 60, COR_BRANCO, 1);
   poligono(1, 3, 180, 120, 100, 80, COR_VERMELHO, 1);


   enviar(i_prior(0));
   frames(2);
   esperar("PRIOR 0", "3 carinhas POR CIMA do retângulo branco e do triângulo vermelho");


   enviar(i_prior(1));
   frames(2);
   esperar("PRIOR 1", "polígonos POR CIMA das carinhas (carinhas somem onde cruzam as formas)");


   for (i = 0; i < 12; i++) { enviar(i_prior(i & 1)); frames(20); }
   esperar("PRIOR alternando", "a prioridade alternou 12 vezes (a cada 20 quadros); terminou com PRIOR=1");
}


/* ---- 10: tudo junto -------------------------------------------------------- */
static void cena_tudo_junto(void)
{
   static const unsigned COR[16] = {
       COR_VERMELHO, COR_VERDE, COR_AZUL, COR_AMARELO, COR_CIANO, COR_MAGENTA,
       COR_LARANJA, COR_BRANCO, 0xE8, 0x3C, 0x6F, 0xB4, 0x5B, 0xD2, 0x27, 0x8D };
   const int N = 600;
   unsigned long inst0;
   double t0, dt;
   int i, t;


   limpar_tela();
   bg_xadrez(COR_AZUL, COR_ROXO, 2);
   bg_moldura(COR_BRANCO);
   for (i = 0; i < N_PADROES; i++) sprite_carregar(i, i);
   for (i = 0; i < N_SPRITES; i++) sprite_cfg(i, i % N_PADROES, 160, 120, (i >> 2) & 1, (i >> 3) & 1, 1);
   for (i = 0; i < N_POLIGONOS; i++) poligono_centro(i, i % 5, 160, 120, 50, 40, COR[i]);
   frames(2);


   esperar("TUDO JUNTO", "vai rodar 600 quadros (~10 s): fundo rolando + 16 polígonos + 32 sprites + PRIOR alternando");


   inst0 = g_enviadas;
   t0 = agora_ms();
   for (t = 0; t < N; t++) {
       enviar(i_bscroll((t / 4) % TILES_X, (t / 8) % TILES_Y));
       for (i = 0; i < N_POLIGONOS; i++)
           poligono_mover_centro(i, i % 5,
               160 + isen(t + i * 4) * 100 / 127, 120 + icos(t * 2 + i * 4) * 70 / 127, 50, 40);
       for (i = 0; i < N_SPRITES; i++)
           sprite_pos(i, 152 + isen(t * (1 + i % 3) + i * 2) * 140 / 127,
                         112 + icos(t * (1 + i % 2) + i * 3) * 100 / 127);
       if (t % 120 == 0) enviar(i_prior((t / 120) & 1));
       frame();
   }
   dt = agora_ms() - t0;
   printf("      %d quadros em %.1f ms -> %.2f fps | %lu instruções (%.1f por quadro)\n",
          N, dt, 1000.0 * N / dt, g_enviadas - inst0, (double)(g_enviadas - inst0) / N);
   printf("      (fps ~60 = o coprocessador acompanhou o VGA com folga)\n");


   esperar("TUDO JUNTO - fim", "animação terminou sem travar; imagem final com tudo visível");
}


/* ---- 11: estresse ---------------------------------------------------------- */
static void cena_estresse(void)
{
   const int N = 30000;
   double t0, dt;
   int i;


   limpar_tela();
   printf("      enviando %d instruções aleatórias válidas (sem NoOp)...\n", N);
   t0 = agora_ms();
   for (i = 0; i < N; i++) {
       switch (rnd() % 11) {
       case 0:  enviar(i_bscroll(rnd() % TILES_X, rnd() % TILES_Y)); break;
       case 1:  enviar(i_btile(rnd() % N_TILES, rnd() & 0xFF)); break;
       case 2:  enviar(i_possp(rnd() % N_SPRITES, rnd() % LARGURA, rnd() % ALTURA)); break;
       case 3:  enviar(i_atsp(rnd() % N_SPRITES, rnd() & 1, rnd() & 1, rnd() & 1)); break;
       case 4:  enviar(i_wrsp(rnd() % 8192, rnd() & 0xFF)); break;
       case 5:  enviar(i_rgsp(rnd() % N_SPRITES, rnd() % N_SPRITE_RAM)); break;
       case 6:  enviar(i_atpo(rnd() % N_POLIGONOS, rnd() % 5, rnd() & 1)); break;
       case 7:  enviar(i_pospo(rnd() % N_POLIGONOS, rnd() % LARGURA, rnd() % ALTURA)); break;
       case 8:  enviar(i_tampo(rnd() % N_POLIGONOS, rnd() % 120, rnd() % 100)); break;
       case 9:  enviar(i_corpo(rnd() % N_POLIGONOS, rnd() & 0xFF)); break;
       default: enviar(i_prior(rnd() & 1)); break;
       }
   }
   dt = agora_ms() - t0;
   printf("      %d instruções em %.1f ms -> %.2f us/instrução (%.0f instr/s), sem timeout\n",
          N, dt, dt * 1000.0 / N, N / (dt / 1000.0));
   frames(2);
   esperar("ESTRESSE", "tela com lixo gráfico aleatório (esperado!): o que importa é não ter travado nem dado timeout");
}


/* ============================================================================
*  main
* ========================================================================== */
typedef struct { const char *nome; void (*func)(void); } Cena;


static const Cena CENAS[] = {
   { "NoOp / VSYNC / opcodes inválidos",     cena_noop },
   { "BTILE (fundo por tiles)",              cena_bg_tiles },
   { "BSCROLL (rolagem do fundo)",           cena_bg_scroll },
   { "Sprites individuais (POSSP/ATSP/RGSP/WRSP)", cena_sprite_individual },
   { "32 sprites simultâneos",               cena_sprites_max },
   { "RAM de sprites cheia (8192 WRSP)",     cena_sprite_ram },
   { "Polígonos individuais (ATPO/POSPO/TAMPO/CORPO)", cena_poligonos_ind },
   { "16 polígonos simultâneos",             cena_poligonos_max },
   { "PRIOR (sprite x polígono)",            cena_prioridade },
   { "Tudo junto (600 quadros)",             cena_tudo_junto },
   { "Estresse (30000 instruções)",          cena_estresse },
};
#define N_CENAS ((int)(sizeof CENAS / sizeof CENAS[0]))


static int validar_padroes(void)
{
   int y;
   for (y = 0; y < 16; y++)
       if (strlen(PAD_F[y]) != 16 || strlen(PAD_CARINHA[y]) != 16) {
           fprintf(stderr, "padrão de sprite com linha %d de tamanho errado\n", y);
           return -1;
       }
   return 0;
}


static void uso(const char *prog)
{
   int i;
   printf("uso: %s [-t ms] [-c cena] [-l]\n"
          "  -t ms   avança sozinho a cada ms milissegundos (padrão: espera ENTER)\n"
          "  -c N    roda só a cena N (1..%d)\n"
          "  -l      lista as cenas\n\ncenas:\n", prog, N_CENAS);
   for (i = 0; i < N_CENAS; i++) printf("  %2d  %s\n", i + 1, CENAS[i].nome);
}


int main(int argc, char **argv)
{
   int i, so_cena = 0;


   for (i = 1; i < argc; i++) {
       if (!strcmp(argv[i], "-t") && i + 1 < argc)      g_tempo_ms = atoi(argv[++i]);
       else if (!strcmp(argv[i], "-c") && i + 1 < argc) so_cena = atoi(argv[++i]);
       else { uso(argv[0]); return !strcmp(argv[i], "-l") ? 0 : 1; }
   }
   if (so_cena < 0 || so_cena > N_CENAS) { uso(argv[0]); return 1; }
   if (validar_padroes() != 0) return 1;


   signal(SIGINT, on_sigint);


   if (gpu_init() != 0) {
       fprintf(stderr, "ERRO: gpu_init falhou (rode com sudo; /dev/mem precisa abrir e mapear a ponte).\n");
       return 1;
   }
   printf("Coprocessador gráfico - bateria de testes (%d cenas). Ctrl+C sai limpando a tela.\n", N_CENAS);


   for (i = 0; i < N_CENAS; i++) {
       if (so_cena && so_cena != i + 1) continue;
       printf("\n==================== CENA %d/%d: %s ====================\n",
              i + 1, N_CENAS, CENAS[i].nome);
       CENAS[i].func();
   }


   printf("\nTodas as cenas concluídas.\n");
   sair_limpo();
   return 0;
}