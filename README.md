<div align="center">
  <figure>
    <img src="resources/titulo.svg">
  </figure>
</div>

<p align="center">
  <img src="https://img.shields.io/badge/Plataforma-DE1--SoC-6d7b48?style=for-the-badge&logo=intel" alt="DE1-SoC">
  <img src="https://img.shields.io/badge/FPGA-Cyclone%20V-6d7b48?style=for-the-badge" alt="Cyclone V">
  <img src="https://img.shields.io/badge/CPU-ARMv7--A-6d7b48?style=for-the-badge&logo=arm" alt="ARM">
  <img src="https://img.shields.io/badge/HDL-Verilog-6d7b48?style=for-the-badge" alt="Verilog">
  <img src="https://img.shields.io/badge/Driver-Assembly-6d7b48?style=for-the-badge&logo=arm" alt="Driver Assembly">
  <img src="https://img.shields.io/badge/Testes-C-6d7b48?style=for-the-badge&logo=c" alt="Testes em C">
</p>

<h3 align="center">CONTRIBUIDORAS</h3>
  
<div align="center">
  <a href="https://github.com/AnaluzLima"><img src="https://avatars.githubusercontent.com/u/200660211?v=4" title="Analuz Lima" width="100" height="100"></a>
  <a href="https://github.com/matoslarissa5025-droid"><img src="https://avatars.githubusercontent.com/u/274644522?v=4" title="Larissa Barbosa" width="100" height="100"></a>
  <a href="https://github.com/vivi2604"><img src="https://avatars.githubusercontent.com/u/263758324?v=4" title="Vivian Moura" width="100" height="100"></a>
</div>

<details><summary><h2 id="1-sobre-o-projeto"> 1. Sobre o projeto</h2></summary> 

Este repositório contém a implementação em Verilog do núcleo de um coprocessador gráfico de 16 bits, projetado para a placa de desenvolvimento Terasic DE1-SoC (Intel Cyclone V SoC). O projeto realiza a renderização de cenários baseados em tiles (background), sprites independentes e polígonos em tempo real a partir de instruções de 32 bits criadas pela equipe e enviadas pelo processador ARM sob o sistema operacional Linux. A comunicação entre o coprocessador gráfico e o processador ARM é regulada pelos sinais do tipo <code>valid</code>, <code>busy</code> e <code>done</code>. 

<details>
  <summary><h2>SUMÁRIO</h2></summary>

  - [1. Sobre o projeto](#1-sobre-o-projeto)
    - [1.1. Requisitos](#11-requisitos)
  - [2. Arquitetura do Sistema](#2-arquitetura-do-sistema)
    - [2.1. Arquitetura do Coprocessador Gráfico em Verilog](#21-arquitetura-do-coprocessador-gráfico-em-verilog)
    - [2.2. Diagrama de Alto Nível](#22-diagrama-de-alto-nível)
    - [2.3. Driver Assembly](#23-driver-assembly)
  - [3. Justificativa das Decisões de Projeto](#3-justificativa-das-decisões-de-projeto)
  - [4. Testes e Validação](#4-testes-e-validação)
    - [4.1. Teste Interativo](#41-teste-interativo)
    - [4.2. Teste Automatizado](#42-teste-automatizado)
    - [4.3. Códigos dos Testes](#43-códigos-dos-testes)
  - [5. Recursos Utilizados da FPGA](#5-recursos-timming-desempenho)
  - [6. Gargalos e Limitações](#6-gargalos-e-limitações)
  - [7. Melhorias Futuras](#7-melhorias-futuras)
  - [8. Tecnologias Utilizadas](#8-tecnologias-utilizadas)
  - [9. Instalação](#9-instalacao)
  - [10. Considerações Finais](#10-considerações-finais)
  - [11. Referências e Inspirações](#11-referencias-inspiracoes)
    - [11.1.Referências](#111-referencias)
    - [11.2.Inspirações](#112-inspiracoes)

</details>

<h3 id="11-requisitos"> 1.1. REQUISITOS</h3>

O desenvolvimento do coprocessador gráfico deve cumprir:
- **Entrada e saída:**
  - A saída de vídeo deverá operar em 640 × 480 pixels, aproximadamente 60 Hz, por meio da interface VGA da DE1-SoC; ✅
  - A resolução lógica da cena deverá ser de 320 × 240 pixels, com duplicação de pixels na saída; ✅
  - Os botões, chaves e LEDs da placa usados exclusivamente para a demonstração do núcleo; ✅
    
- **Núcleo do coprocessador gráfico:**
  - Todo o núcleo deverá ser descrito em Verilog; ✅
  - A arquitetura deverá ser modular; ✅
  - Todos os registradores e memórias deverão possuir estratégia definida de reinicialização ou inicialização; ✅
  - A saída não poderá apresentar instabilidade visual, perda de sincronismo ou pixels indefinidos após a inicialização; ✅
    
- **Motor de background:**
  - Implementar ao menos uma camada de background baseada em tilemap de 40 × 30 entradas; ✅
  - Utilizar tiles de 8 × 8 pixels armazenados em memória interna, com pelo menos 256 padrões disponíveis; ✅
  - Permitir deslocamento horizontal e vertical da camada, com tratamento de repetição ou recorte definido pela equipe; ✅
  - Gerar um índice de cor válido para cada pixel da região visível sem interromper o fluxo de vídeo; ✅
    
- **Motor de sprites:**
  - Disponibilizar memória de atributos para, no mínimo, 32 sprites; ✅
  - Suportar sprites de 16 × 16 pixels, podendo utilizar quatro tiles de 8 × 8 pixels para formar cada imagem; ✅
  - Cada sprite deverá possuir, no mínimo, posição X e Y, índice do padrão gráfico, habilitação, prioridade, espelhamento horizontal, espelhamento vertical e seleção de paleta; ✅
    
- **Rasterizador de polígonos:**
  - Implementar o desenho de triângulos e retângulos preenchidos; ✅
  - Utilizar aritmética inteira; ✅
  
- **Compositor, paleta e saída VGA:**
  - Combinar, a cada pixel, as contribuições do background, da camada de polígonos e dos sprites; ✅
  - Implementar pelo menos três níveis de prioridade entre as camadas; ✅
  - Aplicar transparência antes da seleção do pixel final; ✅
  - Converter o índice de 8 bits por meio de uma paleta programável de 256 entradas RGB; ✅

- **Unidade de busca de instruções;** ✅
- **ISA de 32 bits;** ✅
- **Unidade de Controle;** ✅
- **Datapath;** ✅
- **Banco de registradores;** ✅
- **Registrador de status;** ✅
- **Mecanismo de sincronização com VGA;** ✅
- **Programa de demonstração;** ✅

</details>

---

<details>
<summary><h2 id="2-arquitetura-do-sistema"> 2. Arquitetura do Sistema</h2></summary>

A arquitetura final do coprocessador gráfico é estruturada em blocos funcionais modulares que dividem de forma clara o domínio do gerenciamento das instruções recebidas, o domínio do desenho e o domínio da exibição física do vídeo.

<h3 id="21-arquitetura-do-coprocessador-gráfico-em-verilog"> 2.1. Arquitetura do Coprocessador Gráfico em Verilog</h3>

**2.1.1. Motor de background**

O Motor de Background aplica os parâmetros de deslocamento de rolagem e busca o índice de tile correspondente na memória RAM do Tilemap de 40x30.
- Recebe as coordenadas já na resolução de 320x240;
- Identifica o tipo de instrução que será realizada (<code>BSCROLL</code> ou <code>BTILE</code>);
- Particiona a instrução, gerando os dados necessários;
  - Para o <code>BSCROLL</code>, são os deslocamentos vertical e horizontal;
  - Para o <code>BTILE</code>, são o endereço da memória e a cor;
- Calcula o deslocamento vertical e horizontal;
- Divide a tela em um tilemap de 40x30;
- Calcula o endereço de qual tile o VGA está analisando no momento;
- Manda o endereço para a RAM e devolve a cor do tile;
- Caso a instrução seja para mudar um tile, <code>BTILE</code>, a RAM recebe os dados necessários para tal modificação e, posteriormente, o motor realiza os passos citados acima.

<br>

**2.1.2. Motor de Sprites**

O Motor de Sprites varre seu banco de dados para determinar se algum dos 32 sprites ativos intersecta a coordenada atual, calculando o espelhamento e o deslocamento horizontal ou vertical e acessando a cor do sprite na memória RAM.

- Pega as informações no banco de registradores de todos os 32 sprites e divide em diferentes barramentos;
- Verifica se o sprite está ativo no pixel atual;
- Analisa a ordem de prioridade dos sprites;
- Calcula o espelhamento vertical e horizontal de cada sprite;
- Calcula o endereço do sprite e acessa na memória para saber a cor do pixel;
- Manda o desenho do sprite;
- Caso a instrução seja para mudar o desenho de um sprite na RAM, <code>WRSP</code>, a RAM recebe os dados necessários (o endereço da memória e a cor) para tal modificação e, posteriormente, o motor realiza os passos citados acima.

<br>

**2.1.3. Rasterizador de polígonos**

O rasterizador é responsável por:

- Avaliar simultaneamente o banco de registradores dos 16 polígonos ativos a cada pixel processado pelo Varredor Lógico Interno.
- Suportar a renderização de polígonos preenchidos com tipos pré-definidos em hardware (Retângulo, Quadrado, Triângulo Equilátero, Triângulo Isósceles e Triângulo Escaleno). A ISA permite atualizar dinamicamente a posição (X,Y), o tipo de forma (seletor da geometria), a cor de preenchimento, a prioridade e o estado de visibilidade do polígono no banco de registradores.
- Cálculo de Arestas: Transforma as coordenadas de entrada da varredura interna em números com sinal e executa em paralelo o teste de aresta (produtos vetoriais com sinal) correspondente ao tipo de polígono selecionado, identificando se a coordenada (X,Y) atual está dentro dos limites da forma geométrica.
- Gestão de Prioridades e Transparência: Em áreas onde ocorre sobreposição de múltiplos polígonos, avalia a ordem de prioridade definida nos registradores e envia a cor do polígono visível de maior prioridade para o Compositor.

<br>

**2.1.4. Unidade de Controle**

O módulo Unidade de Controle atua como o cérebro do coprocessador gráfico. Ele implementa uma Máquina de Estados Finitos (FSM) síncrona responsável por orquestrar a busca de comandos na fila (FIFO), decodificar o <code>opcode</code> de 5 bits e gerar os pulsos de habilitação (*enables*) e o barramento de dados para as unidades funcionais (Motores de Background, Sprites e Polígonos).

**Estados da FSM**:

- <code>INIT</code>: Estado ocioso inicial em que a FSM aguarda a chegada de instruções. Permanece neste estado enquanto a fila estiver vazia (<code>fifo_empty = 1</code>). Assim que uma nova instrução é detectada, transita para <code>GET_INST</code>.
- <code>GET_INST</code>: Realiza a retirada da instrução do topo da FIFO ativando o sinal de *pop* (<code>exec_pop = 1</code>). A palavra de 32 bits presente na saída da fila é registrada internamente em instrucao.
- <code>EXEC</code>: Decodifica o <code>opcode</code> localizado nos bits menos significativos (<code>instrucao[4:0]</code>) e isola os operandos nos 27 bits superiores (<code>dados <= instrucao[31:5]</code>). Neste ciclo, ativa de forma exclusiva as linhas de controle do módulo de destino:
  - **Background**: ativa <code>bg_en</code> (<code>BSCROLL</code>) ou <code>bg_ram_en</code> (<code>BTILE</code>).
  - **Sprites**: gera os pulsos decodificados em <code>sprite_en</code> (<code>POSSP</code>, <code>ATSP</code>, <code>RGSP</code>) ou habilita a gravação na memória gráfica com <code>sprite_ram_en</code> (<code>WRSP</code>).
  - **Polígonos**: aciona as linhas de configuração <code>pol_en</code> (<code>ATPO</code>, <code>POSPO</code>, <code>TAMPO</code>, <code>CORPO</code>).
  - **Prioridade do Compositor**: atualiza o registrador prioridade a partir do bit 5 (<code>instrucao[5]</code>).
  - **Transição**: Se a instrução executada for um <code>NOOP</code> (<code>5'b00000</code>), transita para <code>WAIT</code>; caso contrário, se a fila ainda contiver comandos (<code>!fifo_empty</code>), retorna diretamente a <code>GET_INST</code> para manter vazão máxima contínua, ou volta para <code>INIT</code> caso a fila esvazie.
- <code>WAIT</code>: Dedicado exclusivamente à instrução de fim de quadro (<code>NOOP</code>). A Unidade de Controle suspende o fluxo de processamento e aguarda a ocorrência da borda de descida do sinal de sincronismo vertical (<code>negedge vsync</code>). Essa pausa sincroniza o software com a taxa de atualização do monitor (60 Hz), garantindo que as modificações de cena sejam consolidadas durante o apagamento vertical. Ao detectar o evento de <code>VSYNC</code>, a FSM avança para buscar novos comandos ou repousar.

<br>

**2.1.5. Fila de Instruções**

O módulo FIFO atua como a interface primária de entrada de comandos entre o processador principal (ARM Cortex-A9 via registradores <code>PIO</code>/<code>MMIO</code>) e o pipeline gráfico em hardware. Ele combina um banco de armazenamento circular síncrono de 16 palavras com uma Máquina de Estados Finitos (FSM) de handshake de 4 vias (<code>valid</code>, <code>busy</code> e <code>done</code>), garantindo a transferência atômica de instruções sem perda de dados por diferença de clocks.

**Estados da FSM**:

- <code>HAVE_INST</code>: Estado de repouso inicial em que a <code>FIFO</code> mantém <code>done = 0</code> e aguarda uma requisição do HPS. Quando detecta <code>valid = 1</code> e a fila não está cheia (<code>!fifo_full</code>), ativa <code>busy = 1</code>, gera o pulso de escrita <code>wr_en = 1</code> e avança para <code>WRD_INST</code>.
- <code>WRD_INST</code>: Sinaliza a conclusão do ciclo de escrita física do dado na fila. A <code>FSM</code> desativa <code>busy = 0</code> e eleva <code>done = 1</code> para notificar o processador que o comando foi armazenado com sucesso, transitando no ciclo seguinte para <code>WAIT</code>.
- <code>WAIT</code>: Estado de trava síncrona. Mantém <code>done = 1</code> e suspende novas operações até que o processador reconheça a resposta e abaixe a linha de validação (<code>!valid</code>). Assim que o sinal de <code>valid</code> é desassertado, a <code>FSM</code> zera <code>done = 0</code> e retorna para <code>HAVE_INST</code>, ficando pronta para o próximo envio.

**Mecanismo de Fila Circular e Ponteiros**:

- <code>sp</code> (Ponteiro de Escrita): Aponta para a posição da memória onde a próxima instrução vinda do barramento (<code>instr_in</code>) será gravada. Incrementa a cada escrita válida (<code>sp <= sp + 1</code>), retornando automaticamente ao início após a posição 15.
- <code>exec</code> (Ponteiro de Leitura): Aponta para a instrução a ser consumida pela Unidade de Controle. Disponibiliza o dado continuamente na saída através de <code>instr_out = fila[exec]</code> e é incrementado sempre que a UC emite o pulso de retirada (pop) (<code>exec_pop = 1</code>).
- <code>count</code> (Contador de Ocupação): Registrador de 5 bits que monitora o saldo de instruções armazenadas para resolver condições de ambiguidade:
  - Ativa <code>fifo_empty = 1</code> quando <code>count == 0</code>, sinalizando para a Unidade de Controle que não há instruções pendentes.
  - Ativa <code>fifo_full = 1</code> quando <code>count == 16</code>, aplicando contrapressão (backpressure) sobre o HPS para impedir sobreescrita (overflow).
  - Trata inserções simultâneas de escrita e leitura em um mesmo ciclo de clock, mantendo o valor de <code>count</code> estável sem corrupção dos ponteiros.

<br>

**2.1.6. ISA (Instruction Set Architecture)**

A ISA personalizada define o conjunto de instruções de 32 bits suportadas pelo coprocessador. Cada instrução possui um campo de <code>opcode</code> dedicado, seguido dos seus operandos. Os parâmetros empacotados nas instruções representam valores imediatos que alimentam diretamente os registradores internos e controladores de memória:

- <code>X</code> e <code>Y</code> (Posição / Deslocamento): Coordenadas espaciais na tela. Para sprites e polígonos, <code>X</code> possui 9 bits (0 a 319) e <code>Y</code> possui 8 bits (0 a 239). No caso de rolagem do plano de fundo (<code>BSCROLL</code>), representam o deslocamento matricial em unidades de tiles de 8x8 pixels (X de 0 a 39 e Y de 0 a 29).
- <code>COD RAM</code> (Endereçamento de Memória):
  - Em <code>BTILE</code> (11 bits, <code>[23:13]</code>): Endereço linear de uma das 1.200 posições da memória do mapa de tiles (0 a 1199), calculado por <code>Linha * 40 + Coluna</code>.
  - Em <code>WRSP</code> (13 bits, <code>[25:13]</code>): Endereço físico da memória de sprites de 8.192 posições (0 a 8191), cobrindo 32 slots de 256 pixels cada (<code>Slot * 256 + Y * 16 + X</code>).
- <code>cor</code> (Índice de Cor): Valor de 8 bits (0 a 255) codificado no padrão RGB332 (<code>RRRGGGBB</code>). Para sprites e polígonos, o valor <code>0x00</code> atua como transparência.
- <code>ID REG</code> (Identificador do Registrador / Prioridade Fixa): Índice do banco de registradores de destino.
  - Nos sprites, aloca 5 bits (<code>[26:22]</code>, <code>[12:8]</code> ou <code>[14:10]</code>) para selecionar um dos 32 registradores (0 a 31). A posição no banco dita a prioridade estática: o registrador 0 possui a maior prioridade de desenho, sobrepondo o registrador 1, este sobrepondo o 2, até o registrador 31.
  - Nos polígonos, aloca 4 bits (<code>[25:22]</code>, <code>[20:17]</code> ou <code>[16:13]</code>) para selecionar um dos 16 registradores (0 a 15). Assim como nos sprites, o polígono 0 possui a maior prioridade em caso de sobreposição geométrica.
- <code>ID RAM</code> (Identificador do Slot de Sprite): Campo de 5 bits (<code>[9:5]</code>) presente na instrução <code>RGSP</code> que seleciona qual padrão gráfico (dentre os 32 slots de 16x16 pixels armazenados na RAM) será vinculado ao registrador indicado por <code>ID REG</code>. Esse mecanismo desacopla o desenho armazenado da sua prioridade em tela.
- <code>flip</code> (Espelhamento do Sprite): Campo de 2 bits que controla a inversão dos eixos na leitura da memória do sprite:
  - <code>00</code>: Imagem normal.
  - <code>01</code>: Espelhamento vertical (flip V).
  - <code>10</code>: Espelhamento horizontal (flip H).
  - <code>11</code>: Espelhamento simultâneo horizontal e vertical.
- <code>shw</code> (Habilitação de Visibilidade): Bit único (<code>1</code> para visível e <code>0</code> para oculto).
- <code>tipo</code> (Geometria do Polígono): Campo de 3 bits (<code>[7:5]</code>) que seleciona o algoritmo de rasterização por equações de aresta:
  - <code>0</code>: Retângulo;
  - <code>1</code>: Quadrado;
  - <code>2</code>: Triângulo Equilátero;
  - <code>3</code>: Triângulo Isósceles;
  - <code>4</code>: Triângulo Escaleno / Retângulo;
- <code>pri</code> (Prioridade de Camadas): Bit único (<code>instrucao[5]</code>) que orienta o circuito combinacional do Compositor:
  - <code>0</code>: Sprites possuem prioridade sobre os polígonos.
  - <code>1</code>: Polígonos possuem prioridade sobre os sprites.

No total, tem-se 12 instruções que estão representadas abaixo: 

<div align="center">
  <figure>
    <img src="resources/Instrucoes.png">
    <figcaption>
      <i>Figura 1 - Instruções do Coprocessador.</i>
    </figcaption>
  </figure>
</div>

<br>

**2.1.7. Compositor de Camadas**

O módulo Compositor atua como a unidade de varredura lógica, arbitração de camadas e alimentação contínua do Back Buffer. Ele desacopla a geração da imagem da leitura física do monitor, percorrendo autonomamente os 76.800 pixels da resolução de 320x240 no domínio de clock de 25 MHz (<code>clk25</code>) e selecionando a cor final de cada coordenada com base na prioridade configurada e na transparência.

**Estrutura e Funcionamento:**
- **Varredor Virtual de Resolução Lógica:**
  - Implementa dois contadores síncronos internos: <code>count_x</code> (9 bits, variando de 0 a 319) e <code>count_y</code> (8 bits, variando de 0 a 239).
  - Ao atingir o limite horizontal (<code>count_x == 9'd319</code>), <code>count_x</code> é reiniciado em 0 e a linha vertical (<code>count_y</code>) é incrementada. Ao alcançar a última linha (<code>count_y == 8'd239</code>), o quadro reinicia em zero.
  - Os valores dos contadores são exportados continuamente nos barramentos de saída <code>scan_x</code> e <code>scan_y</code>, guiando os três motores gráficos (<code>MotorBG</code>, <code>MotorSprites</code> e <code>rasterizadorPoligonos</code>) na avaliação de suas respectivas geometrias para o pixel corrente.

- **Cálculo de Endereçamento do FrameBuffer:**
  - Converte as coordenadas bidimensionais da varredura em um endereço linear de 17 bits (<code>current_addr</code>) por meio da equação combinacional <code>(count_y * 17'd320) + count_x</code>.
  - O endereço calculado é mapeado na porta de saída <code>fb_write_addr</code>, indexando linearmente a memória de 0 a 76.799.

- **Multiplexação de Camadas, Transparência e Prioridade:**
  - Amostra concorrentemente as saídas de cor dos três motores: <code>draw_bg</code> (plano de fundo), <code>draw_sprites</code> (camada de sprites) e <code>draw_pol</code> (polígonos rasterizados).
  - O índice de cor <code>8'd0</code> (<code>0x00</code>) é tratado como transparente para sprites e polígonos, permitindo que a camada subjacente permaneça visível.
  - A seleção da cor final (<code>sel_cor</code>) é orientada pelo bit de controle <code>prioridade</code> (vindo da instrução <code>PRIOR</code> da ISA):
    - Prioridade Padrão (<code>prioridade == 0</code>): Sprites > Polígonos > Background. Se <code>draw_sprites != 8'd0</code>, a cor do sprite é exibida; caso contrário, se <code>draw_pol != 8'd0</code>, exibe o polígono; caso nenhum elemento ativo intercepte a coordenada, exibe <code>draw_bg</code>.
    - Prioridade Invertida (<code>prioridade == 1</code>): Polígonos > Sprites > Background. Se <code>draw_pol != 8'd0</code>, a cor do polígono se sobrepõe ao sprite; caso contrário, se <code>draw_sprites != 8'd0</code>, exibe o sprite; se ambos forem nulos, exibe <code>draw_bg</code>.

<br>

**2.1.8. Top-Level**

O módulo Top-level integra e conecta todos os subsistemas do coprocessador:

- Conecta a recepção de instruções (via FIFO) à Unidade de Controle;
- Roteia os barramentos de registradores da Unidade de Controle para os Motores Gráficos;
- Interliga a saída combinada do Compositor aos FrameBuffers e sincroniza a leitura do Front Buffer com o Driver VGA.
- Gerencia os domínios de clock através do divisor de frequência, garantindo que o sinal base de 25 MHz alimente o sincronismo VGA, o varredor lógico e o controle síncrono das memórias M10K.

<br>

**2.1.9. Endereço VGA**

O Endereço VGA é um módulo simples responsável pelo cálculo do endereço a ser lido no Front Buffer.

- Recebe as coordenadas reais do VGA;
- Redimensiona a área lógica de trabalho de 640x480 para 320x240;
- Calcula o endereço da memória RAM do Front Buffer.

<br>

**2.1.10. Buffers de Vídeo**

O sistema adota uma arquitetura de Double Buffering (Front Buffer e Back Buffer) implementada em memórias Block RAM de duas portas (Dual-Port BRAM) para evitar tearing (corte de tela) e oscilações de imagem:

- **Back Buffer**: É a memória de destino de escrita contínua. A saída do Compositor é gravada nele à medida que o Varredor Lógico Interno processa os pixels (320x240);
- **Front Buffer**: É a memória de destino de leitura exclusiva do Driver VGA;
- **Alternância de Buffers**: A troca dos papéis de Front e Back Buffer ocorre exatamente no pulso de descida do sinal de sincronismo vertical (<code>VGA_VS</code>), durante o intervalo de apagamento vertical, garantindo que apenas quadros totalmente renderizados sejam exibidos na tela.

<h3 id="22-diagrama-de-alto-nível"> 2.2. Diagrama de Alto Nível</h3>

A Figura 2 abaixo apresenta a arquitetura do projeto estruturada em um Diagrama de Alto Nível.

<div align="center">
  <figure>
    <img src="resources/diagrama.png">
    <figcaption>
      <i>Figura 2 - Diagrama de Alto Nível da Arquitetura do Coprocessador e interface com HPS.</i>
    </figcaption>
  </figure>
</div>

<br>

<h3 id="23-driver-assembly"> 2.3. Driver Assembly</h3>

O driver em Assembly ARMv7-A (32 bits) implementa a camada de comunicação via <code>MMIO</code> entre o processador ARM Cortex-A9 (HPS) e o coprocessador gráfico na FPGA.

<br>

**2.3.1. Mapeamento de Memória (MMIO)**

O acesso ao hardware físico contorna o cache do sistema abrindo <code>/dev/mem</code> e mapeando a ponte Lightweight HPS-to-FPGA via *syscall* <code>mmap2</code>:

- **Endereço Base**: <code>0xFF200000</code> (espaço de 2 MB).
- **Offsets dos Registradores (PIOs)**:
  - <code>PIO_INST</code> (<code>0x00000</code>): Barramento de escrita da instrução (32 bits).
  - <code>PIO_BUSY</code> (<code>0x30000</code>): Entrada indicando ocupação da FIFO.
  - <code>PIO_DONE</code> (<code>0x40000</code>): Entrada sinalizando conclusão da operação na FPGA.
  - <code>PIO_VAL</code> (<code>0x50000</code>): Linha de strobe (valid) gerada pelo HPS.

<br>

**2.3.2. Protocolo de Handshake**

O envio síncrono de instruções na função <code>enviar_instrucao</code> segue um fluxo de 4 vias:
1. **Escrita do Comando**: O HPS escreve o dado em <code>PIO_INST</code>.
2. **Barreira de Memória**: Executa <code>dmb</code> e sobe a linha <code>PIO_VAL</code>.
3. **Reconhecimento da FPGA**: O hardware grava a instrução na FIFO e responde subindo <code>PIO_DONE</code>.
4. **Finalização**: O HPS abaixa <code>PIO_VAL</code> e aguarda <code>PIO_DONE</code> retornar a 0 para liberar a próxima chamada.

<br>

**2.3.3. Interface com C**

As funções criadas no driver podem ser importadas para serem utilizadas em códigos de outras linguagens, como no caso do C. Os procedimentos são:

- <code>int gpu_init(void)</code>: Mapeia o barramento de memória e zera os sinais de controle. Retorna 0 em caso de sucesso ou -1 se houver falha.
- <code>int enviar_instrucao(uint32_t instr)</code>: Transmite a instrução codificada via handshake. Possui um contador interno de timeout para evitar travamentos do sistema caso a placa seja resetada ou não responda.
- <code>void gpu_close(void)</code>: Desfaz o mapeamento da memória e encerra o driver.

</details>

---

<details>
<summary><h2 id="3-justificativa-das-decisões-de-projeto"> 3. Justificativa das Decisões de Projeto</h2></summary>

1. **Imunidade a Inconsistências Visuais**: A técnica de Double Buffering com troca de ponteiros no sincronismo vertical garante que o monitor VGA nunca leia uma tela sendo desenhada pela metade pelos motores, provendo estabilidade visual.
2. **Folga de Desempenho**: Ao desenhar um quadro inteiro em apenas 3,07 ms utilizando o clock de 25 MHz, o hardware consome menos de 20% do orçamento de tempo de varredura física do monitor (16,6 ms), assegurando alto desempenho e margem para fechamento de timing no Quartus.
3. **Aritmética Inteira Signed no Rasterizador**: Parametrizar as coordenadas e os cálculos dos multiplicadores DSP como números inteiros com sinal resolve erros de estouro aritmético quando vértices de polígonos móveis ou estáticos se localizam parcialmente fora do campo visível (coordenadas negativas), eliminando distorções na tela.
4. **Paralelismo de Alta Densidade**: O uso simultâneo de 87 blocos multiplicadores DSP dedicados garante que a validação lógica dos 16 polígonos ocorra em tempo real a cada pixel do varredor, sem atrasos de processamento.
5. **Reformulação do Compositor**: O problema tem como um dos requisitos o desenvolvimento de motores gráficos e compositor, entretanto, sabendo que as suas funcionalidades se assemelham, o compositor foi convertido em um seletor, sendo responsável apenas pela aplicação da prioridade entre motores e a cor final do pixel, enquanto os motores são responsáveis pelos cálculos.
6. **Uso de uma Fila Circular de Instruções**:  Uma fila circular é uma estrutura de dados que permite o reaproveitamento de espaço ao conectar a última posição com a primeira. Com essa abordagem, evita-se o desperdício de espaço e a necessidade de deslocar dados, gerando uma atualização rápida na fila e evitando que novas instruções tenham que esperar para serem armazenadas. Entretanto, vale ressaltar que sua implementação requer mecanismos de controle adicionais, como ponteiros para leitura e escrita e sinais de _status_ (cheio e vazio), para a plena manutenção da fila.
7. **Fases do Ciclo de Execução de Instrução**: Em arquiteturas de processadores, os ciclos de execução de instrução geralmente apresentam quatro fases: busca, decodificação, execução e armazenamento dos resultados. Porém, ao decorrer do desenvolvimento do coprocessador gráfico, percebeu-se a necessidade de modificação neste ciclo, assim, as fases de decodificação e execução foram unidas, já que os motores utilizam apenas um ciclo de _clock_ para realizarem suas tarefas.
8. **Organização das Instruções**: Inicialmente, as instruções foram organizadas de tal modo que os cinco _bits_ mais significativos correspondiam ao <code>opcode</code> e os _bits_ restantes correspondiam às informações necessárias para a instrução. Entretanto, os testes de integração entre o coprocessador e o processador ARM não funcionaram. Isso ocorreu, pois descobriu-se que a passagem sequencial dos dados espelhava a ordem dos _bits_ e as instruções enviadas não eram identificadas pelo coprocessador. Para resolver esse problema, as instruções foram modificadas e o <code>opcode</code> passou a ocupar os cinco _bits_ menos significativos.
9. **Uso da Busca Passiva de Instruções**: A busca de instruções pode ser realizada de duas formas: ativa ou passiva. De acordo com o problema, na busca ativa, uma memória interna disponibiliza as instruções a serem executadas, já na busca passiva, o processador ARM envia as instruções a serem executadas. Neste projeto, foi escolhida a busca passiva devido à futura implementação do jogo em C e a maior simplicidade do código Assembly, em comparação com o da busca ativa.


</details>

---

<details>
<summary><h2 id="4-testes-e-validação"> 4. Testes e Validação</h2></summary>

A validação do coprocessador gráfico foi estruturada em duas abordagens complementares de software em C executando sobre o Linux no processador ARM Cortex-A9 (HPS).

1. **Teste Interativo e Pontual**: Focado no isolamento de cada instrução individual da ISA, permitindo entrada manual de parâmetros e verificação passo a passo do comportamento lógico.
2. **Testes Automatizados e de Estresse**: Desenvolvidos pela Inteligência Artificial Claude para exercitar os limites de hardware do sistema, cobrindo cenários com 32 sprites em movimento, 16 polígonos simultâneos, rolagem contínua de plano de fundo, saturação de FIFO e medições temporais de vazão e taxa de quadros (FPS).

<h3 id="41-teste-interativo">4.1. Teste Interativo</h3>

Disponibiliza um menu no terminal que lê o opcode desejado e seus parâmetros decimais, empacota a palavra de 32 bits de acordo com a especificação e a despacha para o driver Assembly `enviar_instrucao`.

**4.1.1. Instruções**

- Instrução `BSCROLL`: Ajuste de deslocamento horizontal (X de 0 a 39) e vertical (Y de 0 a 29) da matriz de tiles de fundo.
- Instrução `BTILE`: Gravação de cor (0-255) em posições individuais da RAM de tiles (0-1199).
- Instruções de Sprites (`WRSP`, `RGSP`, `POSSP`, `ATSP`): Gravação de pixels na RAM de padrões, associação de registrador ao slot, posicionamento nas coordenadas (X, Y) e configuração de espelhamento (flips) e visibilidade (show).
- Instruções de Polígonos (`POSPO`, `TAMPO`, `ATPO`, `CORPO`): Posicionamento de âncora, definição de largura/altura, seleção de tipo geométrico (retângulo, quadrado, equilátero, isósceles, escaleno) e colorização dinâmica.
- Instrução `PRIOR`: Chaveamento dinâmico da prioridade do compositor entre a camada de polígonos e a camada de sprites.

<div align="center">
  <figure>
    <img src="resources/MenuTesteInst.png">
    <figcaption><br>
      <i>Figura 3 - Terminal exibindo o Menu de seleção da Instrução a ser testada.</i>
    </figcaption>
  </figure>
</div>

<br>

<h3 id="42-teste-automatizado">4.2. Teste Automatizado</h3>

A validação automatizada foi conduzida utilizando dois programas complementares gerados pelo Claude: o `testeClaude1.c`, voltado à validação visual de cenários gráficos complexos e estresse de renderização integrado ao driver Assembly, e o `testeClaude2.c`, realizado anteriormente a implementação do driver Assembly, mas que tem-se guardado o diagnóstico da interface HPS-FPGA e análise de desempenho.

**4.2.1. Teste Visual de Cenas e Estresse (`testeClaude1.c`)**

- **Cena 1 (NoOp / VSYNC / Opcodes Inválidos)**: Validação da sincronização de quadro (60 Hz, cerca de 16,7 ms) via borda de descida do sinal VSYNC e comprovação de imunidade a <code>opcodes</code> não implementados.

<br>

- **Cena 2 (BTILE - Fundo por Tiles)**: Preenchimento total do tilemap (1200 escritas) com padrões xadrez 1x1, 5x5, gradientes de cor e validação de extremidades da tela.

<div align="center">
  <figure>
    <img src="resources/BTILE.gif">
    <figcaption><br>
      <i>Figura 4 - Alteração Dinâmica da RAM de Tiles.</i>
    </figcaption>
  </figure>
</div>

<br>

- **Cena 3 (BSCROLL - Rolagem Contínua)**: Rolagem horizontal e vertical contínuo de tiles.

<div align="center">
  <figure>
    <img src="resources/BSCROLL.gif">
    <figcaption><br>
      <i>Figura 5 - Rolagem Horizontal, Vertical e Diagonal dos Tiles.</i>
    </figcaption>
  </figure>
</div>

<br>

- **Cena 4 (Sprites Individuais)**: Verificação de escrita na RAM de sprites, espelhamentos horizontal e vertical, posicionamento nos 4 cantos e transparência de cor <code>0x00</code>.

<div align="center">
  <figure>
    <img src="resources/SpriteT1.gif">
    <figcaption><br>
      <i>Figura 6 - Testes das Instruções de Alteração dos Sprites.</i>
    </figcaption>
  </figure>
</div>

<br>

- **Cena 5 (32 Sprites Simultâneos)**: Alocação de todos os 32 registradores de sprites com trajetórias e velocidades independentes, avaliando consistência sem perda de objetos na tela.

<div align="center">
  <figure>
    <img src="resources/SpriteT2.gif">
    <figcaption><br>
      <i>Figura 7 - Testes de Alteração dos 32 Sprites Simultaneamente.</i>
    </figcaption>
  </figure>
</div>

<br>

- **Cena 6 (RAM de Sprites Cheia)**: Gravação dos 32 slots da memória de sprites (32 * 256 = 8.192 instruções WRSP consecutivas) e remapeamento dinâmico via RGSP.

<br>

- **Cena 7 (Polígonos Individuais)**: Rasterização em tempo real das 5 formas geométricas suportadas (retângulo, quadrado, triângulos equilátero, isósceles e escaleno), com testes de redimensionamento (TAMPO) e transparência.
  
<div align="center">
  <figure>
    <img src="resources/PoligonosT1.gif">
    <figcaption><br>
      <i>Figura 8 - Testes das Instruções de Alteração dos Polígonos.</i>
    </figcaption>
  </figure>
</div>

<br>

- **Cena 8 (16 Polígonos Simultâneos)**: Renderização de 16 formas orbitando e validação do codificador de prioridade interno.

<div align="center">
  <figure>
    <img src="resources/PoligonosT2.gif">
    <figcaption><br>
      <i>Figura 9 - Testes de Criação e Movimentação de 16 Polígonos Simultaneamente.</i>
    </figcaption>
  </figure>
</div>

<br>

- **Cena 9 (Prioridade de Camadas - PRIOR)**: Sobreposição direta de polígonos e sprites, alternando o registrador de prioridade em tempo real.

<div align="center">
  <figure>
    <img src="resources/PRIOR.gif">
    <figcaption><br>
      <i>Figura 10 - Testes de Prioridade Entre os Motores.</i>
    </figcaption>
  </figure>
</div>

<br>

- **Cena 10 (Cenário Completo Integrado)**: Execução por 600 quadros, combinando rolagem de fundo, 16 polígonos em órbita, 32 sprites em movimento e comutação periódica de prioridade.

<div align="center">
  <figure>
    <img src="resources/ALL.gif">
    <figcaption><br>
      <i>Figura 11 - Teste de Alteração dos Três Motores em conjunto.</i>
    </figcaption>
  </figure>
</div>

<br>

- **Cena 11 (Teste de Estresse Bruto)**: Despacho ininterrupto de 30.000 instruções aleatórias válidas sem inserção de NoOp para validar a saturação da FIFO de entrada e robustez do handshake contra deadlock.

<br>

**4.2.2. Diagnóstico de Comunicação e Resultados de Desempenho (`TesteClaude2.c`)**

Com o objetivo de extrair algumas métricas temporais de cada instrução, foi executado o programa de diagnóstico `TesteClaude2.c` dividido em 8 fases, sem a integração com o driver em Assembly posteriormente desenvolvido pela equipe. Ao todo, foram despachadas **4.091 instruções** monitoradas individualmente no terminal, comprovando a estabilidade do canal MMIO via ponte Lightweight HPS-to-FPGA:

- **Fase 0 - Integridade da Ponte MMIO e Handshake:**
  - **Repouso dos Registradores:** Imediatamente após a liberação do reset (`KEY0`), as linhas de controle estabilizaram em `valid=0`, `busy=0` e `done=0`.
  - **Loopback de Barramento (32 bits):** Padrões binários de estresse injetados no registrador `pio_instrucao` (`0xA5A5A5A5`, `0x5A5A5A5A`, `0xFFFFFFFF`, `0x00000001`, `0x80000000` e `0x00000000`) apresentaram 100% de correspondência na leitura de retorno, atestando a integridade física das trilhas AXI sem curtos ou bits flutuantes.
  - **Latência Unitária de Resposta:** Na primeira instrução despachada, o sinal `done` foi capturado imediatamente com latência de **7,1 µs**.
  - **Vazão Máxima do Barramento:** Durante o envio consecutivo de uma rajada de 1.000 instruções, o tempo total registrado foi de **3,49 ms**, estabelecendo uma taxa média de **3,5 µs por instrução** e uma vazão sustentada de **286.340 instruções/segundo**.

- **Fases 1 a 5 - Latência por Instrução das Unidades Funcionais:**
  - **Plano de Fundo (`BTILE` - Fase 1):** Preenchimento completo dos 1.200 tiles seguido da escrita individual nos 4 cantos (`col=0/39`, `lin=0/29`), confirmando o cálculo de endereçamento linear com latência entre **2,1 µs e 2,4 µs** por instrução.
  - **Rolagem (`BSCROLL` - Fase 2):** Atualização dos registradores de deslocamento horizontal (`0..39`) e vertical (`0..29`) sincronizados ao VSYNC com tempo de resposta entre **2,1 µs e 2,5 µs**.
  - **Polígonos (`TAMPO`, `POSPO`, `CORPO`, `ATPO` - Fase 3):** Configuração e movimentação contínua das 5 primitivas geométricas registrando confirmações estáveis entre **2,1 µs e 2,4 µs** (com pico pontual de **8,3 µs** devido ao escalonamento do Linux).
  - **Sprites (`WRSP`, `RGSP`, `POSSP`, `ATSP` - Fase 4):** Upload de 512 pixels para a RAM de sprites (2 * 256 `WRSP`), vinculação de slots, teste simultâneo das 4 combinações de espelhamento (`flipH` e `flipV`) e animação por 300 quadros com latência entre **2,1 µs e 2,5 µs**.
  - **Prioridade (`PRIOR` - Fase 5):** Alternância dinâmica de sobreposição entre sprite e polígono concluída em **2,1 µs a 2,2 µs**.

- **Fase 6 - Dinâmica da FIFO e Mecanismo de Backpressure:**
  - O estresse da fila interna de 16 posições com o envio o mais rápido possível de 24 comandos `NOOP` (em que cada `NOOP` retém a Unidade de Controle até o próximo VSYNC, cerca de 16,7 ms) comprovou o controle de fluxo por bloqueio.
  - As 17 primeiras instruções foram absorvidas instantaneamente pelo hardware (16 alocadas na fila circular somadas a 1 em execução na Unidade de Controle).
  - O envio **18** foi exatamente o primeiro a ser retido pelo handshake (aguardando **3,8 ms**), comprovando experimentalmente o dimensionamento da arquitetura (`16 na fila + 1 na UC`).
  - O lote de 24 instruções foi completamente drenado em **104,7 ms**, valor perfeitamente alinhado ao tempo esperado de cerca de 7 quadros de vídeo a 60 Hz (aproximadamente 117 ms).

- **Fase 7 e Resumo Global:**
  - O teste foi encerrado limpando os elementos visuais e restaurando os registradores de controle, totalizando **4.091 instruções enviadas** sem nenhum erro de comunicação, com tempo médio global até o sinal `done` de **2.757,7 µs** (média ponderada pelas esperas intencionais de sincronismo de quadro dos comandos `NOOP`).

<h3 id="43-códigos-dos-testes">4.3. Códigos dos Testes</h3>

- **Teste Interativo** ([testeInstrucoes.c](https://github.com/AnaluzLima/Graphics-Coprocessor-for-DE1-SoC-in-Verilog/blob/main/src/testes/testeInstrucoes.c))
- **Teste Automatizado 1** ([testeClaude1.c](https://github.com/AnaluzLima/Graphics-Coprocessor-for-DE1-SoC-in-Verilog/blob/main/src/testes/testeClaude1.c))
- **Teste Automatizado 2** ([testeClaude2.c](https://github.com/AnaluzLima/Graphics-Coprocessor-for-DE1-SoC-in-Verilog/blob/main/src/testes/testeClaude2.c))

</details>

---

<details>
<summary><h2 id="5-recursos-timming-desempenho">5. Análise de recursos, Timing e Desempenho</h2></summary>

De acordo com o relatório de compilação na plataforma Quartus Prime, o projeto utiliza os seguintes recursos da placa DE1-SoC (FPGA Cyclone V 5CSEMA5F31C6):

| Recursos de Hardware | Utilização | Disponível | % de Uso | Justificativas |
| --- | --- | --- | --- | --- |
| **ALMs (Logic Utilization)** | 8.704 | 32.070 | 27% | Lógica combinacional dos 3 motores (decodificação, colisões e produto vetorial) e multiplexadores. |
| **Registradores** | 5.758 | - | - | Bancos de registradores para sprites, polígonos, divisor de clock, driver VGA, etc. |
| **Block Memory Bits (BRAM)** | 1.830.704 | 4.065.280 | 45% | Alocação dos 2 Framebuffers (2 * 614.400 bits), Tilemap (9.600 bits), RAM de Sprites (65.536 bits) e o HPS (526.336 bits). |
| **DSP Blocks** | 87 | 87 | 100% | Multiplicadores dedicados para o cálculo simultâneo das equações de semiplano dos 16 triângulos. |

### **Timming**

- **Domínios de Clock e Frequência Máxima (Fmax):**:
  - <code>clock_50_2</code> (Alimenta o coprocessador gráfico e a lógica de vídeo VGA): Frequência máxima de 64,4 MHz (<code>Restricted Fmax = 64,4 MHz</code>), conferindo folga sobre a meta de operação de 50 MHz da Unidade de Controle e FIFO, e ampla margem sobre o subclock de 25 MHz (<code>clk25</code>) usado pelo Compositor e driver VGA.
  - <code>clock_50_1</code> (Clock do sistema SoC/FPGA): Frequência máxima de 90,68 MHz.
  - <code>afi_clk_write_clk</code> (PLL do controlador de memória SDRAM/DDR3 do HPS): Frequência máxima de 1184,83 MHz (restrita a 717,36 MHz por restrição de período mínimo <code>tmin</code>).
  - <code>altera_reserved_tck</code> (Clock de depuração/JTAG): Frequência máxima de 74,73 MHz.
- **Análise de Folga Temporal (Slack):**
  - **Setup Slack:** Todos os domínios de clock apresentaram folga positiva (positive slack) e Total Negative Slack (TNS) nulo (0,000 ns), garantindo estabilidade e ausência de violações de tempo de estabelecimento:
    - <code>clock_50_2</code>: Folga de +4,472 ns.
    - <code>clock_50_1</code>: Folga de +8,972 ns.
    - <code>afi_clk_write_clk</code>: Folga de +1,730 ns.
    - <code>altera_reserved_tck</code>: Folga de +9,976 ns.
  - **Hold Slack:** As restrições de tempo de retenção foram atendidas com folgas positivas em todos os caminhos (TNS = 0,000 ns), prevenindo condições de corrida:
    - <code>clock_50_2</code>: Folga de +0,374 ns.
    - <code>clock_50_1</code>: Folga de +0,258 ns.
    - <code>afi_clk_write_clk</code>: Folga de +0,143 ns.
    - <code>altera_reserved_tck</code>: Folga de +0,292 ns.
- **Latência de Escrita**: O registrador retém o endereço de gravação por exatos 1 ciclo de _clock_. Esse atraso compensa a latência de leitura e processamento dos motores (MotorBG, MotorSprites e rasterizadorPoligonos), garantindo que o byte de cor resultante seja gravado na mesma célula de memória do Back Buffer.
- **Latência de Leitura**: O driver VGA lê a memória pronta no Front Buffer por meio da porta dedicada de leitura, sem concorrer com a porta de gravação dos motores.

### **Desempenho**

- **Processamento Vetorial Paralelo (DSPs)**: A alocação integral dos 87 blocos de DSP sustenta o cálculo simultâneo de 48 produtos vetoriais a cada pixel do varredor virtual a 25 MHz (16 triângulos x 3 equações de aresta). Isso viabiliza uma taxa contínua de processamento geométrico de 221,18 milhões de produtos vetoriais por segundo em hardware, executada sem interrupções ou consumo de tempo da CPU ARM.
- **Taxa de Preenchimento (Fillrate)**: Em análises sem a implementação de instruções, o varredor lógico desenha os 76.800 pixels da resolução de 320x240 em 76.800 ciclos de 25 MHz, o que equivale a aproximadamente 3,07 milissegundos. Como um quadro completo de 60 Hz dispõe de aproximadamente 16,7 milissegundos, o Back Buffer é renderizado em menos de 20% do tempo total disponível por frame. Já em análises com a implementação de instruções, o sistema atingiu uma vazão de 286.340 instruções por segundo, comportando até 4.772 instruções por frame a 60 FPS.
- **Latência das Instruções**: Como já citado na seção de testes, quando enviadas de forma mais espaçada, as instruções apresentaram um tempo de resposta entre 2,1 e 2,5 µs. Já em um envio consecutivo de uma rajada de 1.000 instruções, o tempo médio aumentou para 3,5 µs, consistente com as esperas pelos sinais de *handshake*. Além disso, vale ressaltar que a primeira instrução apresenta um tempo de resposta de 7,1 µs, revelando um maior gasto de tempo com a configuração inicial do HPS.

</details>

---

<details>
<summary><h2 id="6-gargalos-e-limitações">6. Gargalos e Limitações</h2></summary>

- **Alto Consumo de DSPs**: O cálculo em tempo real das equações de aresta para 16 polígonos paralelos exige uso intensivo dos DSP Blocks da FPGA, sendo o principal fator limitador para expansões futuras;
- **Capacidade física fixa**: O hardware é limitado ao processamento simultâneo de apenas 16 polígonos e 32 sprites.

</details>

---

<details>
<summary><h2 id="7-melhorias-futuras">7. Melhorias Futuras</h2></summary>

- **Backgrounds e Múltiplos Mapas**: Expandir a RAM de Tilemap para suportar múltiplos cenários e permitir a troca dinâmica de mapas em tempo real durante a execução do jogo;
- **Reuso de Multiplicadores**: Aplicar compartilhamento temporal das unidades aritméticas de multiplicação (reuso de DSPs) para reduzir o consumo de hardware e liberar recursos na FPGA, viabilizando o desenho de mais polígonos simultâneos;
- **Ajuste no Boundary Check**: Refinar a lógica de colisão e corte do rasterizador para garantir que polígonos encostados na borda da tela transitem entre diferentes formas sem sobressaltos visuais.

</details>

---

<details>
<summary><h2 id="8-tecnologias-utilizadas">8. Tecnologias Utilizadas</h2></summary>

- Quartus Prime Version 25.1std.0 Build 1129 10/21/2025 SC Lite Edition
- [Excalidraw](https://excalidraw.com/): Esboço do Projeto;
- [Canva](https://www.canva.com/): Diagrama de Alto Nível;
- [Claude](https://claude.ai/new): Códigos de Teste;

</details>

---

<details>
<summary><h2 id="9-instalacao">9. Instalação</h2></summary>

<details>
<summary><h3>Pré-requisitos</h3></summary>

- Possuir instalado o compilador <code>gcc</code>;
- Possuir instalado o <code>Git</code>;
- Possuir instalado o software <code>Quartus Prime</code> (com suporte à família Cyclone V);
- Utilizar uma placa de desenvolvimento FPGA DE1-SoC com Linux embarcado no HPS;
- Possuir um monitor conectado à placa por meio da saída VGA;
- Cabo USB para conexão JTAG e cabo serial/UART ou Ethernet para acesso ao terminal Linux da placa.

</details>

<summary><h3>Passo a Passo</h3></summary>

#### **1. Clonar o repositório**

No terminal do seu computador, clone o repositório do projeto:
```bash
git clone https://github.com/AnaluzLima/Graphics-Coprocessor-for-DE1-SoC-in-Verilog.git
```

#### **2. Compilação e Gravação na FPGA (Quartus Prime)**

1. Abra o Quartus Prime;
2. Vá no menu superior em <code>File > Open Project...</code>;
3. Navegue até a pasta do projeto clonado e abra o arquivo <code>/src/coprocessador/soc_system.qpf</code>
4. Na aba lateral <code>Project Navigator</code>, localize o arquivo <code>ghrd_top.v</code>, clique nele com o botão direito e selecione a opção <code>Set as Top-Level Entity</code>;
5. Inicie a compilação completa do projeto clicando em <code>Processing > Start Compilation</code>;

    > ⚠️ O tempo médio estimado de compilação é de 8 a 12 minutos.

6. Após a conclusão bem-sucedida, conecte a placa DE1-SoC ao computador via cabo USB;
7. Abra o <code>Programmer</code> (<code>Tools > Programmer</code>), certifique-se de que o dispositivo Cyclone V foi detectado e clique em <code>Start</code> para carregar o arquivo <code>.sof</code> na FPGA.

#### **3. Transferência de Arquivos para o HPS (Linux)**

1. Conecte-se ao terminal Linux da DE1-SoC (via SSH, UART/Serial ou SCP);
2. Transfira a biblioteca Assembly e os códigos de teste para o ambiente Linux da placa:
   - <code>/src/libAssembly.S</code>
   - <code>/src/testes/testeClaude1.c</code>
   - <code>/src/testes/testeClaude2.c</code>
   - <code>/src/testes/testeInstrucoes.c</code>

#### **4. Compilação dos Testes no Linux**

No terminal da placa DE1-SoC, dentro do diretório onde os arquivos foram colocados, compile o teste que deseja executar:

- **Para compilar o Teste Visual e Estresse (testeClaude1.c):**
```bash
gcc -O2 -Wall -std=gnu99 testeClaude1.c libAssembly.S -o teste1 -lrt
```

- **Para compilar o Teste de Comunicação e Desempenho (testeClaude2.c):**
```bash
gcc -O2 -std=gnu99 testeClaude2.c -o teste2 -lrt
```

- **Para compilar o Teste Interativo de Instruções (testeInstrucoes.c):**
```bash
gcc -O2 -Wall -std=gnu99 testeInstrucoes.c libAssembly.S -o teste3 -lrt
```

#### **5. Execução dos Testes**
Para acessar a ponte de comunicação <code>MMIO</code> (<code>/dev/mem</code>), você pode executar como superusuário (<code>root</code>).

1. Entre no modo root:
```bash
sudo su
```

2. Execute o binário desejado:
   - Para o teste visual completo:
```bash
./teste1
```

   - Para o teste de diagnóstico e latência (8 fases):
```bash
./teste2
```

   - Para o teste interativo manual:
```bash
./teste3
```

</details>

---

<details>
<summary><h2 id="10-considerações-finais">10. Considerações Finais</h2></summary>

O desenvolvimento deste coprocessador gráfico consolidou com sucesso a integração de uma arquitetura modular em hardware com o ambiente de controle em software. A implementação da ISA de 32 bits, operando em conjunto com a FSM e a FIFO de *handshake*, permitiu o controle autônomo dos motores de *background*, *sprites* e polígonos via <code>MMIO</code> sob o sistema operacional Linux. Aliada a isso, a estratégia de *Double Buffering* sincronizada ao pulso de <code>VSYNC</code> garantiu a entrega estável de 60 quadros por segundo sem a ocorrência de artefatos visuais ou oscilações de sinal. Dessa forma, os resultados alcançados atestam a robustez do núcleo digital e fornecem uma base sólida para as etapas futuras do projeto, que contemplam a aplicação de um jogo interativo em linguagem <code>C</code> e a exploração de melhorias estruturais de hardware.

</details>

---

<details>
<summary><h2 id="11-referencias-inspiracoes">11. Referências e Inspirações</h2></summary>

<h3 id="111-referencias">11.1. Referências</h3>

- ARM LIMITED (2020). Coprocessor and Arm Custom Instructions Interface for Cortex-M33. Documentação Técnica de Arquitetura ARM.
COTTRELL, R. (2004). FPGA Coprocessors: Hardware IP for Software Engineers. White Paper / Artigo Técnico, Altera Corporation, High Wycombe, UK.
- FRANCHINI, S. et al. (2005). A sliced coprocessor for native Clifford algebra operations. In IEEE International Workshop on Computer Architecture for Machine Perception (CAMP),, Palermo, Itália. IEEE.
- HARRIS, D.; HARRIS, S. (2012). Digital Design and Computer Architecture. 2. ed. Waltham, MA: Morgan Kaufmann / Elsevier.
- INTEL CORPORATION / TERASIC (2014). DE1-SoC Development and Media Board User Manual. San Jose, CA: Altera / Terasic Technologies.
- KUMARATHUNGA, D. et al. (2019). VLIW Based Runtime Reconfigurable Machine Vision Coprocessor Architecture for Edge Computing. In IEEE 30th International Conference on Application-Specific Systems, Architectures and Processors (ASAP), IEEE.
- MAAS, E. et al. (1997). A processor-coprocessor architecture for high end video applications. In IEEE International Conference on Computer Design (ICCD), IEEE.
- NEWHOUSE, L.; CUI, H. (2020). Minimalist GPU on an FPGA: Final Report. Relatório de Projeto, Department of Mathematics & EECS, Massachusetts Institute of Technology (MIT), Cambridge, MA.
- PALA, D. (2017). Design and programming of a coprocessor for a RISC-V architecture. Dissertação (Mestrado em Engenharia de Computadores) – Politecnico di Torino, Turim, Itália.
- PATTERSON, D. A.; HENNESSY, J. L. (2017). Computer Organization and Design ARM Edition: The Hardware/Software Interface. Cambridge, MA: Morgan Kaufmann / Elsevier.
- PROJETO DE SISTEMAS DIGITAIS (2026). Otimização de Instruções e Arquitetura para FPGA DE1-SoC. Registro / Documentação do Projeto.
- SUKHSAWAS, S.; BENKRID, K.; CROOKES, D. (2003). A reconfigurable high level FPGA-based coprocessor. In IEEE International Workshop on Computer Architectures for Machine Perception (CAMP), IEEE.
- YANG, R.; RUDOLF, D. R.; RAGHUNATHAN, V. (2006). Flexible Pixel Compositor for Plug-and-Play Multi-Projector Displays. Relatório Técnico, University of Kentucky / Sapphire Computers, Inc.
- YANG, S.; SCHAUMONT, P.; VERBAUWHEDE, I. (2005). Microcoded coprocessor for embedded secure biometric authentication systems. In Proceedings of the 3rd IEEE/ACM/IFIP International Conference on Hardware/Software Codesign and System Synthesis (CODES+ISSS), pp. 240-245, Jersey City, NJ. ACM/IEEE.
- YIU, J. (2010). The Definitive Guide to the ARM Cortex-M3. 2. ed. Burlington, MA: Newnes / Elsevier.


<h3 id="112-inspiracoes">11.2. Inspirações</h3>

- [Readme Template - iuricode](https://github.com/iuricode/readme-template)
- [Awesome Repo Template - MarketingPipeline](https://github.com/MarketingPipeline/Awesome-Repo-Template)
- [coLenda Driver - camilaqPereira](https://github.com/camilaqPereira/coLenda_driver)

</details>
