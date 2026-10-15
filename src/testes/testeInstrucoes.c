#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <signal.h>

extern int gpu_init(void);
extern int enviar_instrucao(uint32_t instr);
extern void gpu_close(void);

#define LARGURA 320
#define ALTURA 240
#define TILES_X 40
#define TILES_Y 30
#define N_TILES (TILES_X * TILES_Y)
#define N_SPRITES 32
#define N_SPRITE_RAM 32
#define SPR_TAM 16
#define N_POLIGONOS 16

enum {NOOP, BSCROLL, BTILE, POSSP, ATSP, WRSP, RGSP, ATPO, POSPO, TAMPO, CORPO, PRIOR};

static inline uint32_t i_noop(void){
   return NOOP;
}
static inline uint32_t i_bscroll(unsigned x, unsigned y){
   return BSCROLL | ((x & 0x3Fu) << 10) | ((y & 0x1Fu) << 5);
}
static inline uint32_t i_btile(unsigned end, unsigned cor){
   return BTILE | ((end & 0x7FFu) << 13) | ((cor & 0xFFu) << 5);
}
static inline uint32_t i_possp(unsigned id, unsigned x, unsigned y){
   return POSSP | ((id & 0x1Fu) << 22) | ((x & 0x1FFu) << 13) | ((y & 0xFFu) << 5);
}
static inline uint32_t i_atsp(unsigned id, unsigned flipv, unsigned fliph, unsigned show){
   return ATSP | ((id & 0x1Fu) << 8) | ((flipv & 1u) << 7) | ((fliph & 1u) << 6) | ((show & 1u) << 5);
}
static inline uint32_t i_wrsp(unsigned end, unsigned cor) {
   return WRSP | ((end & 0x1FFFu) << 13) | ((cor & 0xFFu) << 5);
}
static inline uint32_t i_rgsp(unsigned id, unsigned slot){
   return RGSP | ((id & 0x1Fu) << 10) | ((slot & 0x1Fu) << 5);
}
static inline uint32_t i_atpo(unsigned id, unsigned tipo, unsigned show){
   return ATPO | ((id & 0xFu) << 9) | ((show & 1u) << 8) | ((tipo & 7u) << 5);
}
static inline uint32_t i_pospo(unsigned id, unsigned x, unsigned y){
   return POSPO | ((id & 0xFu) << 22) | ((x & 0x1FFu) << 13) | ((y & 0xFFu) << 5);
}
static inline uint32_t i_tampo(unsigned id, unsigned larg, unsigned alt){
   return TAMPO | ((id & 0xFu) << 22) | ((larg & 0x1FFu) << 13) | ((alt & 0xFFu) << 5);
}
static inline uint32_t i_corpo(unsigned id, unsigned cor){
   return CORPO | ((id & 0xFu) << 13) | ((cor & 0xFFu) << 5);
}
static inline uint32_t i_prior(unsigned p){
   return PRIOR | ((p & 1u) << 5);
}

static void enviar(uint32_t instr){
   if (enviar_instrucao(instr) != 0) fprintf(stderr, "ERRO: sem 'done' do coprocessador (instrucao 0x%08X)\n", instr);
}

int main(){
   if (gpu_init() != 0) {
       fprintf(stderr, "Erro ao inicializar a GPU\n");
       return EXIT_FAILURE;
   }

   int opcode;
   int x, y, id, cor, larg, alt, flipv, fliph, show, end, reg, slot, tipo, prioridade, tile, pol;
   printf("\033[1;33mTESTE DE ENTRADA DE INSTRUÇÕES PARA O COPROCESSADOR\033[m\n");
   printf("Para o teste funcionar, digite os números corretamente, sendo um usuário perfeito.\n");
   printf("\033[1;31mCaso digite algum número fora do intervalo, o programa pode se comportar de maneira inesperada.\033[m\n\n");

   while(1){
       printf("\033[1;36mQual instrução deseja testar?\033[m \n[1] NOOP\n[2] BSCROLL\n[3] BTILE\n[4] POSSP\n[5] ATSP\n[6] WRSP\n[7] RGSP\n[8] ATPO\n[9] POSPO\n[10] TAMPO\n[11] CORPO\n[12] PRIOR\n[0] Sair\n\033[m");
       scanf("%d", &opcode);

       printf("\n");

       switch(opcode){
           case 1:
               enviar(i_noop());
               break;
           case 2:
               printf("Digite os valores de x e y para BSCROLL (0-39 para x, 0-29 para y): \n");
              
               printf("X: ");
               scanf("%d", &x);

               printf("\nY: ");
               scanf("%d", &y);

               enviar(i_bscroll(x, y));
               break;
           case 3:
               printf("Digite o valor do tile que voce deseja mudar (0-1199): \n");
               scanf("%d", &tile);

               printf("\nDigite a cor do tile (0-255): \n");
               scanf("%d", &cor);
              
               enviar(i_btile(tile, cor));
               break;
           case 4:
               printf("Digite os valores de x e y para POSSP (0-319 para x, 0-239 para y): \n");
              
               printf("ID (0-31): \n");
               scanf("%d", &id);
              
               printf("X: \n");
               scanf("%d", &x);
              
               printf("Y: ");
               scanf("%d", &y);
              
               enviar(i_possp(id,x,y));
               break;
           case 5:
               printf("Digite os valores de flipv, fliph e show para ATSP (0 ou 1): \n");
              
               printf("ID (0-31): \n");
               scanf("%d", &id);
              
               printf("Flip Vertical (0 ou 1): \n");
               scanf("%d", &flipv);               
              
               printf("Flip Horizontal (0 ou 1): \n");
               scanf("%d", &fliph);
              
               printf("Show (0 ou 1): \n");
               scanf("%d", &show);
              
               enviar(i_atsp(id,flipv,fliph,show));
               break;
           case 6:
               printf("Digite o valor do endereço de WRSP (0-8191): \n");
              
               scanf("%d", &end);
              
               printf("Digite a cor do pixel (0-255): \n");
              
               scanf("%d", &cor);
              
               enviar(i_wrsp(end, cor));
               break;
           case 7:
              
               printf("Digite o valor do registrador (0-31): \n");
               scanf("%d", &reg);
              
               printf("Digite o valor do slot na RAM (0-31): \n");
               scanf("%d", &slot);


               enviar(i_rgsp(reg, slot));
               break;
           case 8:
               printf("Digite os valores de ID, tipo e show para ATPO: \n");
              
               printf("ID (0-15): \n");
               scanf("%d", &id);
              
               printf("\nTipo (0-4): \n");
               scanf("%d", &tipo);


               printf("\nShow (0 ou 1): \n");
               scanf("%d", &show);

               enviar(i_atpo(id, tipo, show));
               break;
           case 9:
               printf("Digite os valores de x e y para POSPO (0-319 para x, 0-239 para y): \n");

               printf("ID (0-15): \n");
               scanf("%d", &id);

               printf("X: ");
               scanf("%d", &x);

               printf("\nY: ");
               scanf("%d", &y);
              
               enviar(i_pospo(id, x, y));
               break;
           case 10:
               printf("Definindo o tamanho do polígono: \n");

               printf("ID (0-15): \n");
               scanf("%d", &id);
              
               printf("Largura (1-319): \n");
               scanf("%d", &larg);

               printf("\nAltura (1-239): \n");
               scanf("%d", &alt);

               enviar(i_tampo(id, larg, alt));
               break;
           case 11:
               printf("Definindo a cor do polígono: \n");

               printf("Poligono (0-15): \n");
               scanf("%d", &pol);
              
               printf("Cor (0-255): \n");
               scanf("%d", &cor);

               enviar(i_corpo(pol, cor));
               break;
           case 12:
               printf("Defina a prioridade entre os motorores:\n0. Sprite tem prioridade sobre polígono\n1. Polígono tem prioridade sobre sprite\n");
              
               scanf("%d", &prioridade);
              
               enviar(i_prior(prioridade));
               break;
           case 0:
               printf("Saindo...\n");
               gpu_close();
               return EXIT_SUCCESS;
           default:
               printf("Opção inválida. Tente novamente.\n");
       }
   }

   gpu_close();
   return EXIT_SUCCESS;
}