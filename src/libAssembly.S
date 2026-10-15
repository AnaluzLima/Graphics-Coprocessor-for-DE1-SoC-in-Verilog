.syntax unified @instrui o montador a usar a sintaxe moderna unificada do ARM
.arm @força a geração de instruções no modo ARM nativo de 32 bits

.global gpu_init
.global gpu_close
.global enviar_instrucao

@informa explicitamente ao linker que esses rótulos representam funções executáveis
.type gpu_init, %function
.type gpu_close, %function
.type enviar_instrucao, %function

.equ CLOSE, 6 @syscall para fechar arquivo
.equ OPEN, 5 @syscall para abrir arquivo
.equ MUNMAP, 91 @syscall para desmapear memoria
.equ MMAP2, 192 @syscall para mapear memoria

.equ O_RDWR_SYNC, 0x101002 @garante que qualquer escrita seja despachada diretamente para o barramento físico
.equ PROT_RW, 3 @permitir leitura e escrita na memoria mapeada
.equ MAP_SHARED,  1 @permitir compartilhamento de memoria

.equ LW_BRIDGE_PAGE, 0xFF200 @ 0xFF200000 >> 12 (mmap2 usa páginas de 4 KB)
.equ LW_SPAN, 0x200000 @ 2 MB (cobre até o offset 0x5000F)

.equ PIO_INST, 0x00000
.equ PIO_BUSY, 0x30000
.equ PIO_DONE, 0x40000
.equ PIO_VAL,  0x50000

.equ TIMEOUT, 0x1000000 @iterações de laço para evitar que o processador congele caso a FPGA não responda

.data @declarar váriveis globais dinâmicas
.align 2 @ptr_base precisa estar alinhado em 4 bytes
   dev_mem: .asciz "/dev/mem" @nome do arquivo de memoria
.align 2 @sem isso ptr_base ficava no offset 9 (.data)
   ptr_base: .word 0 @variavel para armazenar o ponteiro base do mapeamento de memoria

.text @onde o código executável será escrito
.align 2

gpu_init:
   push {r4-r7, lr} @salva os registradores e lr na pilha

   ldr r1, =ptr_base @carregar o endereço da variável ptr_base
   ldr r0, [r1] @armazenar o ponteiro base do mapeamento de memoria na variável ptr_base
   cmp r0, #0 @se r0 for != 0, significa que a gpu já foi inicializada
   bne init_ok

   mov r7, #OPEN @carregar o numero da syscall para abrir arquivo
   ldr r0, =dev_mem @carregar o endereço do nome do arquivo
   ldr r1, =O_RDWR_SYNC @carregar a flag para abrir arquivo em leitura e escrita
   mov r2, #0 @Como o arquivo `/dev/mem` já existe no Linux e só estamis abrindo, o valor é irrelevante
   svc 0

   cmp r0, #0
   blt erro_init @se for menor que 0, houve erro na abertura do arquivo
   mov r4, r0 @armazenar o descritor do arquivo em r4

   mov r7, #MMAP2 @carregar o numero da syscall para mapear memoria
   mov r0, #0 @o linux escolhe o endereço
   ldr r1, =LW_SPAN @2 MB
   mov r2, #PROT_RW @carregar a flag para permitir leitura e escrita na memoria mapeada
   mov r3, #MAP_SHARED @carregar a flag para permitir compartilhamento de memoria
   ldr r5, =LW_BRIDGE_PAGE @carregar o endereço base do bridge
   svc 0

   cmn r0, #4096 @se r0 < 0, o resultado da soma será negativo
   bhi erro_mmap @se for menor que 0, houve erro no mmap

   ldr r1, =ptr_base @carregar o endereço da variável ptr_base
   str r0, [r1] @armazenar o ponteiro base do mapeamento de memoria na variável ptr_base

   mov r7, #CLOSE @fecha o descritor do arquivo
   mov r0, r4 @descritor do arquivo
   svc 0

init_ok:
   @valid = 0
   ldr r0, =ptr_base
   ldr r0, [r0] @carregar o ponteiro base do mapeamento de memoria
   mov r3, #0 @inicializar valid como 0
   add r2, r0, #PIO_VAL @carregar o endereço do registrador valid
   str r3, [r2]

   mov r0, #0 @operação bem sucedida
   pop {r4-r7, pc} @pega os valores dos registradores e retorna para o endereço de retorno

erro_mmap:
   mov r7, #CLOSE @não vaza o descritor se o mmap falhar
   mov r0, r4 @descritor do arquivo
   svc 0
erro_init:
   mvn r0, #0 @-1
   pop {r4-r7, pc}

gpu_close:
   push {r7, lr} @salva os registradores e lr na pilha
   ldr r1, =ptr_base
   ldr r0, [r1]
   cmp r0, #0 @verifica se gpu_init foi chamada
   beq close_fim @se não foi chamada, não faz nada

   mov r2, #0 @zera a variável ptr_base
   str r2, [r1]
   ldr r1, =LW_SPAN
   mov r7, #MUNMAP @carregar o numero da syscall para desmapear memoria
   svc 0

close_fim:
   pop {r7, pc} @pega os valores dos registradores e retorna para o endereço de retorno

enviar_instrucao:
   ldr r1, =ptr_base
   ldr r1, [r1]
   cmp r1, #0
   beq erro_envio @se gpu_init não foi chamada, não faz nada

   add r2, r1, #PIO_VAL @carregar o endereço do registrador valid
   add r3, r1, #PIO_DONE @carregar o endereço do registrador done

   str r0, [r1, #PIO_INST] @escreve a instrução no registrador de instruções
   dmb @garante que a escrita da instrução seja feita antes de escrever o valid
   mov r0, #1
   str r0, [r2] @escreve 1 no registrador de valor para iniciar a execução da instrução

   ldr r12, =TIMEOUT @carrega o valor do timeout

espera_done_alto:
   ldr r0, [r3] @lê o registrador de done
   tst r0, #1 @compara com 1
   bne done_alto @se for igual a 1, sai do loop
   subs r12, r12, #1 @decrementa o timeout
   bne espera_done_alto @se não for igual a 1, continua esperando
   b timeout @se o timeout chegar a 0, sai do loop e vai para a rotina de timeout

done_alto:
   mov r0, #0 @valid
   str r0, [r2] @escreve 0 no registrador de valor para finalizar a execução da instrução
   ldr r12, =TIMEOUT @carrega o valor do timeout

espera_done_baixo:
   ldr r0, [r3] @lê o registrador de done
   tst r0, #1 @compara com 1
   beq envio_ok @se for igual a 1, sai do loop e vai para a rotina de envio_ok
   subs r12, r12, #1 @decrementa o timeout
   bne espera_done_baixo @se não for igual a 1, continua esperando

timeout:
   mov r0, #0 @valid
   str r0, [r2] @escreve 0 no registrador de valor para finalizar a execução da instrução

erro_envio:
   mvn r0, #0 @-1
   bx lr @retorna para a função chamadora com erro

envio_ok:
   mov r0, #0 @operação bem sucedida
   bx lr @retorna para a função chamadora com sucesso

.ltorg @força o montador a despejar a tabela literal de constantes
@informa ao linker e ao kernel que a pilha do programa não precisa de permissão de execução de código
.section .note.GNU-stack,"",%progbits