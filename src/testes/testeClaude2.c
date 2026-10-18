/* =====================================================================
 * testeClaude2.c
 * Teste de comunicação HPS (ARM/Linux) -> coprocessador gráfico (FPGA)
 * DE1-SoC  |  TEC 499 - MI Sistemas Digitais  |  Problema 2
 *
 * O que faz:
 *   Envia instruções da ISA do coprocessador pela ponte Lightweight
 *   HPS-to-FPGA usando o handshake valid/busy/done da FIFO, e deixa você
 *   ACOMPANHAR cada instrução rodando, de duas formas:
 *     - modo manual (padrão): cada instrução só é enviada quando você
 *       aperta ENTER/espaço no terminal;
 *     - modo automático (-t ms): um temporizador avança sozinho.
 *
 * Compilar na placa:   gcc -O2 -Wall -o teste testeClaude2.c
 * Rodar (precisa root): sudo ./teste            (passo a passo)
 *                       sudo ./teste -t 400     (automático, 400 ms)
 *                       sudo ./teste -f 4       (só a fase 4)
 * Testar no PC (sem placa): make sim && ./teste_sim -t 1
 *
 * ANTES DE RODAR: o .sof precisa estar carregado na FPGA (acessar a
 * ponte sem FPGA configurada trava o HPS) e a KEY0 solta (é o reset).
 *
 * Teclas: [ENTER/espaço] avança   [a] alterna manual/automático
 *         [+] mais rápido  [-] mais lento  [p] pula a fase  [q] sai
 *
 * Esta é a camada que o driver Assembly vai substituir: tudo que está
 * em "enviar()" e nas funções mk_*()/i_*() é o que o driver ARM terá
 * que fazer a partir de uma única chamada em C.
 * ===================================================================== */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/mman.h>

/* ---------------------------------------------------------------------
 * Mapa de hardware (vem do hps_0.h gerado pelo Platform Designer)
 * ------------------------------------------------------------------- */
#define LW_BRIDGE_BASE 0xFF200000u
#define LW_BRIDGE_SPAN 0x00200000u   /* 2 MB: cobre até o pio_valid (0x50000) */

#if defined(__has_include)
#  if __has_include("hps_0.h")
#    include "hps_0.h"
#  endif
#endif
#ifndef PIO_INSTRUCAO_BASE
#  define PIO_INSTRUCAO_BASE 0x00000   /* HPS -> FPGA  (32 bits)  */
#endif
#ifndef PIO_BUSY_BASE
#  define PIO_BUSY_BASE      0x30000   /* FPGA -> HPS  (1 bit)    */
#endif
#ifndef PIO_DONE_BASE
#  define PIO_DONE_BASE      0x40000   /* FPGA -> HPS  (1 bit)    */
#endif
#ifndef PIO_VALID_BASE
#  define PIO_VALID_BASE     0x50000   /* HPS -> FPGA  (1 bit)    */
#endif

#define TIMEOUT_NS 1500000000ull       /* 1,5 s sem resposta = erro */

/* ---------------------------------------------------------------------
 * ISA do coprocessador: instrução de 32 bits
 *     [31:5] dados (27 bits)   [4:0] opcode
 * ------------------------------------------------------------------- */
enum {
    OP_NOP     = 0,   /* espera a borda de descida do vsync (sincroniza quadro) */
    OP_BSCROLL = 1,   /* dados: x[10:5] (6b)  y[4:0] (5b)                        */
    OP_BTILE   = 2,   /* dados: addr[18:8] (11b, 0..1199)  cor[7:0]              */
    OP_POSSP   = 3,   /* dados: id[21:17]  x[16:8] (9b)  y[7:0]                  */
    OP_ATSP    = 4,   /* dados: id[7:3]  flipV[2]  flipH[1]  show[0]             */
    OP_WRSP    = 5,   /* dados: addr[20:8] (13b = ram*256+y*16+x)  cor[7:0]      */
    OP_RGSP    = 6,   /* dados: id[9:5]  id_da_ram[4:0]                          */
    OP_ATPO    = 7,   /* dados: id[15:12]  show[11]  tipo[10:8]  cor[7:0]        */
    OP_POSPO   = 8,   /* dados: id[20:17]  x[16:8]  y[7:0]                       */
    OP_TAMPO   = 9,   /* dados: id[20:17]  largura[16:8]  altura[7:0]            */
    OP_CORPO   = 10,  /* dados: id[11:8]  cor[7:0] */
    OP_PRIOR   = 11   /* dados: bit0 da fatia de dados = bit5 da instrução */
};
enum { POL_RET = 0, POL_QUAD = 1, POL_EQUI = 2, POL_ISOS = 3, POL_ESCA = 4 };

/* cores RGB332 (cor 0 = transparente em sprites e polígonos) */
#define RGB332(r,g,b) ((uint8_t)((((r)&7)<<5)|(((g)&7)<<2)|((b)&3)))
#define COR_VERMELHO 0xE0
#define COR_VERDE    0x1C
#define COR_AZUL     0x03
#define COR_AMARELO  0xFC
#define COR_MAGENTA  0xE3
#define COR_BRANCO   0xFF
#define COR_CEU_A    RGB332(3,5,3)
#define COR_CEU_B    RGB332(2,4,3)
#define COR_GRAMA_A  RGB332(1,6,0)
#define COR_GRAMA_B  RGB332(1,5,0)

static inline uint32_t mk(unsigned op, uint32_t dados)
{
    return ((dados & 0x07FFFFFFu) << 5) | (op & 0x1Fu);
}

/* ---------------------------------------------------------------------
 * Camada de acesso aos PIOs (real, via /dev/mem, ou simulada com -DSIM)
 * ------------------------------------------------------------------- */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
#define BARREIRA() __sync_synchronize()

#ifndef SIM
/* ------------------------- hardware real ---------------------------- */
static volatile uint32_t *r_instr, *r_busy, *r_done, *r_valid;
static void *g_lw = MAP_FAILED;
static int   g_fd = -1;

static int hw_init(void)
{
    g_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_fd < 0) {
        perror("open(/dev/mem)");
        fprintf(stderr, "Dica: execute com sudo.\n");
        return -1;
    }
    g_lw = mmap(NULL, LW_BRIDGE_SPAN, PROT_READ | PROT_WRITE, MAP_SHARED,
                g_fd, LW_BRIDGE_BASE);
    if (g_lw == MAP_FAILED) {
        perror("mmap");
        close(g_fd);
        g_fd = -1;
        return -1;
    }
    r_instr = (volatile uint32_t *)((uint8_t *)g_lw + PIO_INSTRUCAO_BASE);
    r_busy  = (volatile uint32_t *)((uint8_t *)g_lw + PIO_BUSY_BASE);
    r_done  = (volatile uint32_t *)((uint8_t *)g_lw + PIO_DONE_BASE);
    r_valid = (volatile uint32_t *)((uint8_t *)g_lw + PIO_VALID_BASE);
    return 0;
}
static void hw_close(void)
{
    if (r_valid) *r_valid = 0;
    if (g_lw != MAP_FAILED) munmap(g_lw, LW_BRIDGE_SPAN);
    if (g_fd >= 0) close(g_fd);
    g_lw = MAP_FAILED;
    g_fd = -1;
    r_valid = NULL;
}
static inline void     hw_instr_write(uint32_t w) { *r_instr = w; }
static inline uint32_t hw_instr_read(void)        { return *r_instr; }
static inline uint32_t hw_busy(void)              { return *r_busy & 1u; }
static inline uint32_t hw_done(void)              { return *r_done & 1u; }
static inline uint32_t hw_valid_get(void)         { return *r_valid & 1u; }
static inline void     hw_valid_set(uint32_t v)   { *r_valid = v & 1u; }

#else
/* ---------------- modelo simulado (só para testar no PC) ------------ */
/* Imita a FIFO de 16 posições + UC: NOP segura a UC por 1 quadro (16,7 ms). */
static uint32_t sim_instr, sim_valid, sim_done;
static uint32_t sim_q[16];
static int      sim_cnt, sim_wait_low;
static uint64_t sim_free_at;

static void sim_update(void)
{
    uint64_t t = now_ns();
    while (sim_cnt > 0 && t >= sim_free_at) {
        uint32_t w = sim_q[0];
        memmove(&sim_q[0], &sim_q[1], (size_t)(15) * sizeof sim_q[0]);
        sim_cnt--;
        sim_free_at = ((w & 0x1F) == OP_NOP) ? t + 16666667ull : t;
    }
    if (!sim_wait_low && sim_valid && sim_cnt < 16) {
        sim_q[sim_cnt++] = sim_instr;
        sim_done = 1;
        sim_wait_low = 1;
    }
    if (sim_wait_low && !sim_valid) {
        sim_done = 0;
        sim_wait_low = 0;
    }
}
static int  hw_init(void)  { fprintf(stderr, "[SIM] modelo simulado, sem hardware\n"); return 0; }
static void hw_close(void) { sim_valid = 0; }
static inline void     hw_instr_write(uint32_t w) { sim_instr = w; }
static inline uint32_t hw_instr_read(void)        { return sim_instr; }
static inline uint32_t hw_busy(void)              { return 0; }
static inline uint32_t hw_done(void)              { sim_update(); return sim_done; }
static inline uint32_t hw_valid_get(void)         { return sim_valid; }
static inline void     hw_valid_set(uint32_t v)   { sim_valid = v & 1u; sim_update(); }
#endif

/* ---------------------------------------------------------------------
 * Terminal: leitura de tecla sem ENTER + temporizador
 * ------------------------------------------------------------------- */
static struct termios g_tio_old;
static int g_tty_raw;
static int g_auto;            /* 1 = avanço por temporizador */
static int g_delay_ms = 500;
static int g_pular;           /* 1 = abandona o restante da fase atual */

static void tty_raw(void)
{
    struct termios t;
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &g_tio_old) != 0) return;
    t = g_tio_old;
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == 0) g_tty_raw = 1;
}
static void tty_restore(void)
{
    if (g_tty_raw) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_tio_old);
        g_tty_raw = 0;
    }
}

/* >0: tecla | 0: timeout/interrupção | -1: fim da entrada. ms<0 = bloqueia */
static int tecla(int ms)
{
    fd_set fds;
    struct timeval tv, *ptv = NULL;
    unsigned char c;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if (ms >= 0) {
        tv.tv_sec = ms / 1000;
        tv.tv_usec = (ms % 1000) * 1000;
        ptv = &tv;
    }
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, ptv) <= 0) return 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) return -1;
    return c;
}

/* Espera o "avanço": tecla (manual) ou temporizador (automático). */
static void aguardar(void)
{
    for (;;) {
        int k = tecla(g_auto ? g_delay_ms : -1);
        if (k == 0) {
            if (g_auto) return;      /* temporizador estourou */
            continue;                /* interrupção de sinal: espera de novo */
        }
        if (k < 0) { g_auto = 1; return; }   /* stdin acabou: segue sozinho */
        switch (k) {
        case ' ': case '\n': case '\r':
            return;
        case 'a': case 'A':
            g_auto = !g_auto;
            printf("\n  [modo %s", g_auto ? "AUTOMATICO" : "MANUAL");
            if (g_auto) printf(", %d ms", g_delay_ms);
            printf("]\n");
            fflush(stdout);
            if (g_auto) return;
            break;
        case '+':
            g_delay_ms = g_delay_ms / 2 < 10 ? 10 : g_delay_ms / 2;
            printf("\n  [intervalo: %d ms]\n", g_delay_ms);
            fflush(stdout);
            break;
        case '-':
            g_delay_ms = g_delay_ms * 2 > 5000 ? 5000 : g_delay_ms * 2;
            printf("\n  [intervalo: %d ms]\n", g_delay_ms);
            fflush(stdout);
            break;
        case 'p': case 'P':
            g_pular = 1;
            return;
        case 'q': case 'Q':
            printf("\n  [saindo]\n");
            exit(0);
        default:
            break;
        }
    }
}

/* ---------------------------------------------------------------------
 * Envio de instruções (handshake valid/busy/done)
 *
 *   HPS: escreve a instrução em pio_instrucao  ->  valid = 1
 *   FPGA (FIFO): copia a instrução, sobe done (busy fica 1 por 1 ciclo)
 *   HPS: vê done = 1  ->  valid = 0
 *   FPGA: done = 0  (ciclo fechado, pode mandar a próxima)
 *
 * Se a FIFO (16 posições) estiver cheia, a FIFO simplesmente não aceita:
 * done não sobe e o HPS espera ("backpressure").
 * ------------------------------------------------------------------- */
static unsigned g_n_env, g_n_polls_done, g_n_polls_fim;
static uint64_t g_ns_total, g_last_dt;
static int g_quiet;           /* 1 = não imprime nem espera tecla (blocos/animações) */

static int enviar(uint32_t w)
{
    uint64_t t0 = now_ns();
    unsigned n = 0, m = 0;

    hw_instr_write(w);
    BARREIRA();
    hw_valid_set(1);
    while (!hw_done()) {
        if ((++n & 0xFFu) == 0 && now_ns() - t0 > TIMEOUT_NS) {
            hw_valid_set(0);
            return -1;
        }
    }
    g_last_dt = now_ns() - t0;
    BARREIRA();
    hw_valid_set(0);
    while (hw_done()) {
        if ((++m & 0xFFu) == 0 && now_ns() - t0 > TIMEOUT_NS) return -2;
    }
    g_n_polls_done = n;
    g_n_polls_fim = m;
    g_n_env++;
    g_ns_total += g_last_dt;
    return 0;
}

static void falha_comunicacao(int r)
{
    printf("\n\n  *** ERRO DE COMUNICACAO (%s) ***\n",
           r == -1 ? "done nunca subiu" : "done nunca desceu");
    printf("  Confira: (1) .sof carregado na FPGA?  (2) KEY0 solta (e o reset)?\n"
           "           (3) pio_instrucao como OUTPUT no Platform Designer?\n"
           "           (4) hps_0.h com os enderecos atuais?\n");
    exit(2);
}

/* Monta, descreve, espera o avanço e envia. */
static void emit(unsigned op, uint32_t dados, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
static void emit(unsigned op, uint32_t dados, const char *fmt, ...)
{
    uint32_t w = mk(op, dados);
    int r;
    if (g_pular) return;
    if (!g_quiet) {
        char desc[160];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(desc, sizeof desc, fmt, ap);
        va_end(ap);
        printf("  #%03u  0x%08X  %-46s", g_n_env + 1, w, desc);
        fflush(stdout);
        aguardar();
        if (g_pular) { printf("-> (fase pulada)\n"); return; }
    }
    r = enviar(w);
    if (r) falha_comunicacao(r);
    if (!g_quiet)
        printf("-> ok  %.1f us (done apos %u leituras)\n",
               (double)g_last_dt / 1000.0, g_n_polls_done);
}
/* versão silenciosa para laços/animações */
static void emit_q(unsigned op, uint32_t dados)
{
    int r;
    if (g_pular) return;
    r = enviar(mk(op, dados));
    if (r) falha_comunicacao(r);
}

/* Espera o avanço sem enviar nada (para blocos e explicações). */
static void esperar(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void esperar(const char *fmt, ...)
{
    va_list ap;
    if (g_pular) return;
    printf("  >> ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    aguardar();
    printf("\n");
}

/* ---------------------------------------------------------------------
 * Instruções da ISA (uma função por mnemônico)
 * ------------------------------------------------------------------- */
static void i_nop(void) { emit(OP_NOP, 0, "NOP        (espera vsync)"); }
static void frames(int n) { while (n-- > 0 && !g_pular) emit_q(OP_NOP, 0); }

static uint32_t d_bscroll(int x, int y)
{ return ((uint32_t)(x & 0x3F) << 5) | (uint32_t)(y & 0x1F); }
static void i_bscroll(int x, int y)
{ emit(OP_BSCROLL, d_bscroll(x, y), "BSCROLL    x=%d y=%d", x, y); }

static uint32_t d_btile(int col, int lin, uint8_t cor)
{ return ((uint32_t)((lin * 40 + col) & 0x7FF) << 8) | cor; }
static void i_btile(int col, int lin, uint8_t cor)
{ emit(OP_BTILE, d_btile(col, lin, cor), "BTILE      col=%d lin=%d cor=0x%02X", col, lin, cor); }

static uint32_t d_possp(int id, int x, int y)
{ return ((uint32_t)(id & 0x1F) << 17) | ((uint32_t)(x & 0x1FF) << 8) | (uint32_t)(y & 0xFF); }
static void i_possp(int id, int x, int y)
{ emit(OP_POSSP, d_possp(id, x, y), "POSSP      sprite=%d x=%d y=%d", id, x, y); }

static void i_atsp(int id, int flip_h, int flip_v, int show)
{
    uint32_t d = ((uint32_t)(id & 0x1F) << 3) | ((uint32_t)(flip_v & 1) << 2) |
                 ((uint32_t)(flip_h & 1) << 1) | (uint32_t)(show & 1);
    emit(OP_ATSP, d, "ATSP       sprite=%d flipH=%d flipV=%d show=%d", id, flip_h, flip_v, show);
}
static void i_wrsp(int ram, int x, int y, uint8_t cor)
{
    uint32_t addr = ((uint32_t)ram << 8) | ((uint32_t)y << 4) | (uint32_t)x;
    emit(OP_WRSP, ((addr & 0x1FFF) << 8) | cor, "WRSP       ram=%d (x=%d,y=%d) cor=0x%02X", ram, x, y, cor);
}
static void i_rgsp(int id, int ram)
{
    emit(OP_RGSP, ((uint32_t)(id & 0x1F) << 5) | (uint32_t)(ram & 0x1F),
         "RGSP       sprite=%d <- imagem da ram %d", id, ram);
}
static void i_atpo(int id, int tipo, int show)
{
    uint32_t d = ((uint32_t)(id & 0xF) << 4) | ((uint32_t)(show & 1) << 3) | (uint32_t)(tipo & 7);
    emit(OP_ATPO, d, "ATPO       poligono=%d tipo=%d show=%d", id, tipo, show);
}

static void i_corpo(int id, uint8_t cor)
{
    uint32_t d = ((uint32_t)(id & 0xF) << 8) | (uint32_t)cor;
    emit(OP_CORPO, d, "CORPO      poligono=%d cor=0x%02X", id, cor);
}
static uint32_t d_pospo(int id, int x, int y)
{ return ((uint32_t)(id & 0xF) << 17) | ((uint32_t)(x & 0x1FF) << 8) | (uint32_t)(y & 0xFF); }
static void i_pospo(int id, int x, int y)
{ emit(OP_POSPO, d_pospo(id, x, y), "POSPO      poligono=%d x=%d y=%d", id, x, y); }
static void i_tampo(int id, int w, int h)
{
    emit(OP_TAMPO, ((uint32_t)(id & 0xF) << 17) | ((uint32_t)(w & 0x1FF) << 8) | (uint32_t)(h & 0xFF),
         "TAMPO      poligono=%d largura=%d altura=%d", id, w, h);
}
static void i_prior(int p)
{ emit(OP_PRIOR, (uint32_t)(p & 1), "PRIOR      %d (%s na frente)", p & 1, (p & 1) ? "poligono" : "sprite"); }

/* ---------------------------------------------------------------------
 * Sprites de teste (16x16, '.' = transparente)
 * ------------------------------------------------------------------- */
static const char *const SPR_CARINHA[16] = {
    "....YYYYYYYY....", "..YYYYYYYYYYYY..", ".YYYYYYYYYYYYYY.", ".YYYKKYYYYKKYYY.",
    "YYYYKKYYYYKKYYYY", "YYYYYYYYYYYYYYYY", "YYYYYYYYYYYYYYYY", "YYYYYYYYYYYYYYYY",
    "YYKYYYYYYYYYYKYY", "YYYKYYYYYYYYKYYY", "YYYYKYYYYYYKYYYY", "YYYYYKKKKKKYYYYY",
    "YYYYYYYYYYYYYYYY", ".YYYYYYYYYYYYYY.", "..YYYYYYYYYYYY..", "....YYYYYYYY...."
};
/* metade de cima vermelha, de baixo azul: deixa os flips H e V visíveis */
static const char *const SPR_SETA[16] = {
    "................", "........R.......", "........RR......", "........RRR.....",
    "RRRRRRRRRRRR....", "RRRRRRRRRRRRR...", "RRRRRRRRRRRRRR..", "RRRRRRRRRRRRRRR.",
    "BBBBBBBBBBBBBBB.", "BBBBBBBBBBBBBB..", "BBBBBBBBBBBBB...", "BBBBBBBBBBBB....",
    "........BBB.....", "........BB......", "........B.......", "................"
};
static uint8_t cor_de(char c)
{
    switch (c) {
    case 'Y': return COR_AMARELO;
    case 'K': return 0x24;
    case 'R': return COR_VERMELHO;
    case 'B': return COR_AZUL;
    case 'G': return COR_VERDE;
    case 'W': return COR_BRANCO;
    default:  return 0;
    }
}
static void upload_sprite(int ram, const char *const bmp[16])
{
    int x, y;
    for (y = 0; y < 16 && !g_pular; y++)
        for (x = 0; x < 16; x++)
            i_wrsp(ram, x, y, cor_de(bmp[y][x]));
}
static int g_sprites_na_ram;
static void garantir_sprites(void)
{
    if (g_sprites_na_ram) return;
    esperar("Enviando imagens dos sprites para a RAM (2 x 256 WRSP)");
    g_quiet = 1;
    upload_sprite(0, SPR_CARINHA);
    upload_sprite(1, SPR_SETA);
    g_quiet = 0;
    if (!g_pular) g_sprites_na_ram = 1;
}

/* ---------------------------------------------------------------------
 * Fases de teste
 * ------------------------------------------------------------------- */
static void fase(int n, const char *titulo, const char *dica)
{
    g_pular = 0;
    printf("\n==============================================================\n");
    printf(" FASE %d - %s\n", n, titulo);
    if (dica && *dica) printf(" %s\n", dica);
    printf("==============================================================\n");
    esperar("ENTER para iniciar a fase");
}

/* ---- Fase 0: diagnóstico do handshake ---- */
static void fase0(void)
{
    static const uint32_t pad[] = { 0xA5A5A5A5u, 0x5A5A5A5Au, 0xFFFFFFFFu,
                                    0x00000001u, 0x80000000u, 0x00000000u };
    unsigned i, ok = 1;
    uint64_t t0, dt;
    fase(0, "Comunicacao HPS <-> FPGA", "valid/busy/done, loopback do pio_instrucao e vazao");

    esperar("Estado de repouso dos PIOs (esperado: valid=0 busy=0 done=0)");
    printf("     valid=%u busy=%u done=%u  -> %s\n", hw_valid_get(), hw_busy(), hw_done(),
           (hw_valid_get() | hw_busy() | hw_done()) ? "ATENCAO: nao esta em repouso" : "ok");

    esperar("Loopback do pio_instrucao (escreve padroes e le de volta)");
    for (i = 0; i < sizeof pad / sizeof pad[0]; i++) {
        uint32_t r;
        hw_instr_write(pad[i]);
        BARREIRA();
        r = hw_instr_read();
        printf("     escreveu 0x%08X  leu 0x%08X  %s\n", pad[i], r, r == pad[i] ? "ok" : "DIFERENTE");
        if (r != pad[i]) ok = 0;
    }
    hw_instr_write(0);
    if (!ok)
        printf("     *** O HPS nao esta conseguindo escrever em pio_instrucao. ***\n"
               "     Provavel causa: o PIO esta como INPUT no Platform Designer.\n"
               "     Mude para OUTPUT, gere o HDL, recompile e regere o hps_0.h.\n");

    esperar("Handshake completo de 1 instrucao (PRIOR 0 e inofensiva)");
    i_prior(0);

    esperar("Vazao: 1000 x PRIOR 0 em sequencia");
    if (!g_pular) {
        g_quiet = 1;
        t0 = now_ns();
        for (i = 0; i < 1000; i++) emit_q(OP_PRIOR, 0);
        dt = now_ns() - t0;
        g_quiet = 0;
        printf("     1000 instrucoes em %.2f ms  ->  %.1f us/instr  (%.0f instr/s)\n",
               (double)dt / 1e6, (double)dt / 1000.0 / 1000.0, 1000.0 * 1e9 / (double)dt);
    }
}

/* ---- Fase 1: plano de fundo ---- */
static void fase1(void)
{
    int col, lin;
    fase(1, "Plano de fundo (BTILE)", "tilemap 40x30, tiles de 8x8 pixels, cor RGB332");

    esperar("Pinta o tilemap inteiro (1200 BTILE): ceu listrado + grama");
    if (!g_pular) {
        g_quiet = 1;
        for (lin = 0; lin < 30 && !g_pular; lin++)
            for (col = 0; col < 40; col++) {
                uint8_t cor = (lin < 22) ? (((col / 4) & 1) ? COR_CEU_B : COR_CEU_A)
                                         : (((col + lin) & 1) ? COR_GRAMA_B : COR_GRAMA_A);
                emit_q(OP_BTILE, d_btile(col, lin, cor));
            }
        g_quiet = 0;
    }
    esperar("Agora os 4 cantos, um a um, para conferir o enderecamento");
    i_btile(0, 0, COR_VERMELHO);     /* topo-esquerda  */
    i_btile(39, 0, COR_AMARELO);     /* topo-direita   */
    i_btile(0, 29, COR_AZUL);        /* baixo-esquerda */
    i_btile(39, 29, COR_BRANCO);     /* baixo-direita  */
}

/* ---- Fase 2: scroll ---- */
static void fase2(void)
{
    int i;
    fase(2, "Rolagem do fundo (BSCROLL)", "o scroll vale a partir do proximo vsync");
    i_bscroll(1, 0);
    i_bscroll(0, 1);
    i_bscroll(5, 3);
    i_bscroll(0, 0);

    esperar("Animacao: rolagem horizontal 0..39 (3 quadros por passo)");
    if (!g_pular) {
        g_quiet = 1;
        for (i = 0; i < 40 && !g_pular; i++) {
            emit_q(OP_BSCROLL, d_bscroll(i, 0));
            frames(3);
            printf("\r     scroll x = %2d ", i); fflush(stdout);
        }
        printf("\n");
        g_quiet = 0;
    }
    esperar("Animacao: rolagem vertical 0..29");
    if (!g_pular) {
        g_quiet = 1;
        for (i = 0; i < 30 && !g_pular; i++) {
            emit_q(OP_BSCROLL, d_bscroll(0, i));
            frames(3);
            printf("\r     scroll y = %2d ", i); fflush(stdout);
        }
        printf("\n");
        g_quiet = 0;
    }
    i_bscroll(0, 0);
}

/* ---- Fase 3: polígonos ---- */
static void config_poligono(int id, int tipo, int x, int y, int w, int h, uint8_t cor)
{
    i_tampo(id, w, h);
    i_pospo(id, x, y);
    i_corpo(id, cor);
    i_atpo(id, tipo, 1);  /* ativa a visibilidade */
}
static void fase3(void)
{
    int x;
    fase(3, "Poligonos (TAMPO, POSPO, ATPO)", "retangulo, quadrado e 3 triangulos; cada um = 3 instrucoes");
    config_poligono(0, POL_RET,  20,  20, 60, 30, COR_VERMELHO);
    config_poligono(1, POL_QUAD, 100, 20, 40,  0, COR_VERDE);
    config_poligono(2, POL_EQUI, 190, 20, 40, 35, COR_AZUL);
    config_poligono(3, POL_ISOS, 260, 20, 40, 45, COR_AMARELO);
    config_poligono(4, POL_ESCA,  20, 120, 60, 50, COR_MAGENTA);

    esperar("Animacao: move o retangulo (POSPO a cada quadro)");
    if (!g_pular) {
        g_quiet = 1;
        for (x = 20; x <= 220 && !g_pular; x += 2) {
            emit_q(OP_POSPO, d_pospo(0, x, 20 + (x - 20) / 4));
            frames(1);
        }
        for (x = 220; x >= 20 && !g_pular; x -= 2) {
            emit_q(OP_POSPO, d_pospo(0, x, 20 + (x - 20) / 4));
            frames(1);
        }
        g_quiet = 0;
    }
    i_pospo(0, 20, 20);
    i_atpo(0, POL_RET, 0);   /* esconde (sem parâmetro de cor) */
    i_atpo(0, POL_RET, 1);   /* mostra de novo */
}

/* ---- Fase 4: sprites ---- */
static void fase4(void)
{
    int f, x = 0, y = 60, dx = 3, dy = 2, k;
    fase(4, "Sprites (WRSP, RGSP, POSSP, ATSP)", "imagem na RAM -> vincula ao sprite -> posiciona -> mostra");
    garantir_sprites();
    i_rgsp(0, 0);
    i_possp(0, 140, 90);
    i_atsp(0, 0, 0, 1);

    esperar("4 sprites com a mesma imagem (ram 1): normal, flipH, flipV, flipH+V");
    for (k = 0; k < 4; k++) {
        i_rgsp(1 + k, 1);
        i_possp(1 + k, 60 + 50 * k, 170);
    }
    i_atsp(1, 0, 0, 1);
    i_atsp(2, 1, 0, 1);
    i_atsp(3, 0, 1, 1);
    i_atsp(4, 1, 1, 1);

    esperar("Animacao: carinha quicando (POSSP + NOP a cada quadro, 300 quadros)");
    if (!g_pular) {
        g_quiet = 1;
        for (f = 0; f < 300 && !g_pular; f++) {
            x += dx; if (x <= 0 || x >= 304) dx = -dx;
            y += dy; if (y <= 0 || y >= 224) dy = -dy;
            emit_q(OP_POSSP, d_possp(0, x, y));
            frames(1);
        }
        g_quiet = 0;
    }
    i_possp(0, 140, 90);
}

/* ---- Fase 5: prioridade ---- */
static void fase5(void)
{
    fase(5, "Prioridade sprite x poligono (PRIOR)", "um retangulo grande por cima/baixo da carinha");
    garantir_sprites();
    i_rgsp(0, 0);
    i_atsp(0, 0, 0, 1);
    config_poligono(0, POL_RET, 120, 90, 80, 60, COR_VERMELHO);
    i_possp(0, 150, 105);
    i_prior(0);
    i_prior(1);
    i_prior(0);
}

/* ---- Fase 6: FIFO cheia ---- */
static void fase6(void)
{
    enum { N = 24 };
    uint64_t dt[N], t0, tot;
    int i, primeira = -1;
    fase(6, "Fila de instrucoes (FIFO de 16)", "NOP segura a UC por 1 quadro (~16,7 ms): a fila enche e o envio bloqueia");
    esperar("Envia %d NOPs o mais rapido possivel", N);
    if (g_pular) return;
    g_quiet = 1;
    t0 = now_ns();
    for (i = 0; i < N; i++) {
        emit_q(OP_NOP, 0);
        dt[i] = g_last_dt;
        if (primeira < 0 && dt[i] > 2000000ull) primeira = i + 1;
    }
    tot = now_ns() - t0;
    g_quiet = 0;
    printf("     total: %.1f ms para %d NOPs\n", (double)tot / 1e6, N);
    if (primeira > 0)
        printf("     o envio #%d foi o primeiro a esperar (%.1f ms) - esperado ~#18 (16 na fila + 1 na UC)\n",
               primeira, (double)dt[primeira - 1] / 1e6);
    else
        printf("     nenhum envio precisou esperar (FIFO nao encheu?)\n");
    printf("     esperado ~%.0f ms (cerca de %d quadros de 16,7 ms)\n", (N - 17) * 16.7, N - 17);
}

/* ---- Fase 7: limpeza ---- */
static void fase7(void)
{
    int i;
    fase(7, "Limpeza", "esconde sprites e poligonos, zera scroll e prioridade");
    esperar("Esconder 5 sprites + 5 poligonos, BSCROLL 0,0 e PRIOR 0");
    if (g_pular) return;
    g_quiet = 1;
    for (i = 0; i < 5; i++) {
        emit_q(OP_ATSP, ((uint32_t)i << 3));
        emit_q(OP_ATPO, ((uint32_t)i << 4) | 0); /* show = 0 */
    }
    emit_q(OP_BSCROLL, 0);
    emit_q(OP_PRIOR, 0);
    g_quiet = 0;
    printf("     feito.\n");
}

/* ---------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------- */
static void limpar(void)
{
    tty_restore();
    hw_close();
}
static void on_signal(int s)
{
    (void)s;
    tty_restore();
    hw_close();
    _exit(130);
}
static void uso(const char *p)
{
    printf("Uso: %s [-t ms] [-f fase] [-h]\n"
           "  -t ms    modo automatico: avanca sozinho a cada ms milissegundos\n"
           "  -f fase  roda so uma fase (0..7)\n"
           "Fases: 0 comunicacao | 1 fundo | 2 scroll | 3 poligonos | 4 sprites\n"
           "       5 prioridade | 6 fila (FIFO) | 7 limpeza\n", p);
}

int main(int argc, char **argv)
{
    static void (*const fases[])(void) = { fase0, fase1, fase2, fase3, fase4, fase5, fase6, fase7 };
    int i, so = -1;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-t") && i + 1 < argc) {
            g_delay_ms = atoi(argv[++i]);
            if (g_delay_ms < 1) g_delay_ms = 1;
            g_auto = 1;
        } else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
            so = atoi(argv[++i]);
            if (so < 0 || so > 7) { uso(argv[0]); return 1; }
        } else {
            uso(argv[0]);
            return !strcmp(argv[i], "-h");
        }
    }
    if (!isatty(STDIN_FILENO)) g_auto = 1;

    atexit(limpar);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    if (hw_init() != 0) return 1;
    tty_raw();

    printf("Teste do coprocessador grafico - %s\n", g_auto ? "AUTOMATICO" : "MANUAL");
    printf("  [ENTER/espaco] avanca  [a] manual/auto  [+/-] velocidade  [p] pula fase  [q] sai\n");

    if (so >= 0) fases[so]();
    else for (i = 0; i < 8; i++) fases[i]();

    printf("\n--------------------------------------------------------------\n");
    printf(" Resumo: %u instrucoes enviadas", g_n_env);
    if (g_n_env) printf(", tempo medio ate done = %.1f us", (double)g_ns_total / g_n_env / 1000.0);
    printf("\n--------------------------------------------------------------\n");
    return 0;
}
