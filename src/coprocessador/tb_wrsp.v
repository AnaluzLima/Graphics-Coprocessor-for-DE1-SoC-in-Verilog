// ============================================================================
//  tb_wrsp.v  -  Testbench do WRSP (escrita dinâmica na RAM de sprites)
//
//  Instancia o `main` REAL do projeto (FIFO + UC + RegSprites + MotorSprites +
//  RAM + Compositor + FrameBuffers) e envia as instruções com o MESMO handshake
//  valid/done do assembly.S. Não usa nenhum modelo próprio da lógica: só as RAMs
//  (altsyncram) vêm da biblioteca do simulador.
//
//  O que o teste mostra no console:
//    - [UC]  cada WRSP decodificado pela UC (sprite_ram_en + dados_inst)
//    - [RAM] o barramento de escrita DENTRO do MotorSprites (wr_en/wr_addr/wr_data)
//    - quadros do sprite 16x16 como lido do framebuffer (R=vermelho G=verde B=azul
//      Y=amarelo W=branco C=ciano M=magenta  .=igual ao fundo/transparente)
//    - PASSOU/FALHOU de cada pixel e o resultado final
//
//  Verilog-2001 puro: roda no ModelSim/Questa (com -L altera_mf_ver) e no Icarus.
// ============================================================================
`timescale 1ns/1ps

module tb_wrsp;

    // ---- clocks e reset (50 MHz do sistema, 25 MHz do pixel) ----------------
    reg clk = 0, clk25 = 0, rst = 1, vsync = 1;
    always #10 clk   = ~clk;
    always #20 clk25 = ~clk25;

    // ---- interface com a FIFO (igual ao PIO do HPS) -------------------------
    reg  [31:0] cmd   = 32'd0;
    reg         valid = 1'b0;
    wire        busy, done;
    wire [7:0]  cor_final;

    main dut (
        .cmd_in     (cmd),
        .reset      (rst),
        .clk        (clk),
        .clk25      (clk25),
        .vsync      (vsync),
        .vga_raw_x  (10'd0),
        .vga_raw_y  (10'd0),
        .valid_FIFO (valid),
        .busy_FIFO  (busy),
        .done_FIFO  (done),
        .corFinal   (cor_final)
    );

    // vsync: nível baixo por 200 ciclos a cada 160000 (~1 quadro de 320x240 a 25 MHz)
    integer vc = 0;
    always @(posedge clk) begin
        vc    <= vc + 1;
        vsync <= !((vc % 160000) > 159800);
    end

    // ---- captura do quadro que o compositor escreve no framebuffer ----------
    reg [7:0] fb   [0:76799];
    reg [7:0] bgfb [0:76799];
    always @(posedge clk25)
        if (dut.fb_write_en && dut.fb_write_addr < 17'd76800)
            fb[dut.fb_write_addr] <= dut.fb_write_data;

    // ---- monitores: mostram o caminho do WRSP --------------------------------
    integer mostrar = 0;          // quantas linhas de log ainda podem ser impressas
    integer erros_ligacao = 0;
    always @(posedge clk) if (!rst) begin
        if (dut.sprite_ram_en) begin
            if (mostrar > 0)
                $display("   [UC ] ciclo=%0d sprite_ram_en=1 dados_inst=0x%07h (end=%0d cor=0x%02h) | MotorSprites.dados=0x%07h",
                         vc, dut.dados_inst, dut.dados_inst[20:8], dut.dados_inst[7:0], dut.sp.dados);
            if (dut.sp.dados !== dut.dados_inst) erros_ligacao = erros_ligacao + 1;
        end
        if (dut.sp.wr_en && mostrar > 0) begin
            $display("   [RAM] ciclo=%0d wr_en=1 wr_addr=%0d wr_data=0x%02h  -> slot %0d, pixel (x=%0d, y=%0d)",
                     vc, dut.sp.wr_addr, dut.sp.wr_data,
                     dut.sp.wr_addr[12:8], dut.sp.wr_addr[3:0], dut.sp.wr_addr[7:4]);
            mostrar = mostrar - 1;
        end
    end

    // ---- instruções (mesmos campos do testeC.c: opcode em [4:0]) -------------
    function [31:0] i_possp(input [4:0] id, input [8:0] x, input [7:0] y);
        i_possp = 32'd3 | (id << 22) | (x << 13) | (y << 5);
    endfunction
    function [31:0] i_atsp(input [4:0] id, input v, input h, input s);
        i_atsp = 32'd4 | (id << 8) | (v << 7) | (h << 6) | (s << 5);
    endfunction
    function [31:0] i_wrsp(input [12:0] a, input [7:0] c);
        i_wrsp = 32'd5 | (a << 13) | (c << 5);
    endfunction
    function [31:0] i_rgsp(input [4:0] id, input [4:0] s);
        i_rgsp = 32'd6 | (id << 10) | (s << 5);
    endfunction

    integer ninst = 0, erros = 0, acertos = 0;

    // handshake idêntico ao assembly.S: instr, valid=1, espera done=1, valid=0, espera done=0
    task enviar(input [31:0] instr);
        integer n;
        begin
            cmd = instr;
            @(posedge clk); valid <= 1'b1;
            n = 0;
            while (!done && n < 5000) begin @(posedge clk); n = n + 1; end
            if (!done) begin $display("   *** TIMEOUT esperando done (instr 0x%08h)", instr); erros = erros + 1; end
            valid <= 1'b0;
            n = 0;
            while (done && n < 5000) begin @(posedge clk); n = n + 1; end
            ninst = ninst + 1;
        end
    endtask

    task wrsp(input integer slot, input integer x, input integer y, input [7:0] c);
        begin enviar(i_wrsp(slot * 256 + y * 16 + x, c)); end
    endtask

    task quadro;                       // espera o compositor varrer um quadro inteiro
        begin repeat (162000) @(posedge clk); end
    endtask

    // ---- leitura do sprite no framebuffer -------------------------------------
    localparam SX = 40, SY = 30;       // posição do sprite 0
    integer d = 0;                     // deslocamento horizontal do pipeline (descoberto na calibração)

    function [7:0] pix(input integer x, input integer y);
        pix = fb[(SY + y) * 320 + SX + x + d];
    endfunction
    function [7:0] pixbg(input integer x, input integer y);
        pixbg = bgfb[(SY + y) * 320 + SX + x + d];
    endfunction

    function [7:0] simb(input [7:0] c, input [7:0] b);
        begin
            if (c === b) simb = ".";
            else case (c)
                8'hE0: simb = "R";  8'h1C: simb = "G";  8'h03: simb = "B";
                8'hFC: simb = "Y";  8'hFF: simb = "W";  8'h1F: simb = "C";
                8'hE3: simb = "M";  default: simb = "?";
            endcase
        end
    endfunction

    task mostrar_sprite(input [8*44-1:0] titulo);
        integer x, y;
        begin
            $display("   --- %0s ---", titulo);
            for (y = 0; y < 16; y = y + 1) begin
                $write("       ");
                for (x = 0; x < 16; x = x + 1) $write("%0s ", simb(pix(x, y), pixbg(x, y)));
                $write("\n");
            end
        end
    endtask

    task chk(input integer x, input integer y, input [7:0] esp, input [8*30-1:0] nome);
        begin
            if (pix(x, y) === esp) begin
                acertos = acertos + 1;
                $display("   ok     %0s  pixel(%0d,%0d) = 0x%02h", nome, x, y, esp);
            end else begin
                erros = erros + 1;
                $display("   FALHOU %0s  pixel(%0d,%0d): leu 0x%02h, esperado 0x%02h", nome, x, y, pix(x, y), esp);
            end
        end
    endtask

    task chk_transp(input integer x, input integer y, input [8*30-1:0] nome);
        begin
            if (pix(x, y) === pixbg(x, y)) begin
                acertos = acertos + 1;
                $display("   ok     %0s  pixel(%0d,%0d) = fundo (transparente)", nome, x, y);
            end else begin
                erros = erros + 1;
                $display("   FALHOU %0s  pixel(%0d,%0d): leu 0x%02h, esperado fundo 0x%02h", nome, x, y, pix(x, y), pixbg(x, y));
            end
        end
    endtask

    // ---- roteiro ---------------------------------------------------------------
    integer k, c;
    initial begin
        $dumpfile("tb_wrsp.vcd");
        $dumpvars(1, tb_wrsp);
        $dumpvars(1, dut.fifo);
        $dumpvars(1, dut.uc);
        $dumpvars(1, dut.sp);
        for (k = 0; k < 76800; k = k + 1) begin fb[k] = 8'h00; bgfb[k] = 8'h00; end

        $display("");
        $display("================ TESTBENCH WRSP ================");
        repeat (5) @(posedge clk); rst = 1'b0; repeat (5) @(posedge clk);

        // ---- P0: fundo ------------------------------------------------------
        quadro; quadro;
        for (k = 0; k < 76800; k = k + 1) bgfb[k] = fb[k];
        $display("[P0] reset solto, fundo capturado (sprites ainda escondidos)");

        // ---- P1: configura sprite 0 -> slot 3 em (40,30) ----------------------
        $display("[P1] RGSP(sprite 0 -> slot 3) + POSSP(40,30) + ATSP(show=1), SEM escrever na RAM");
        enviar(i_rgsp(0, 3));
        enviar(i_possp(0, SX, SY));
        enviar(i_atsp(0, 0, 0, 1));
        quadro;
        mostrar_sprite("P1: slot 3 como veio da inicializacao");

        // ---- P2: WRSP em 16 pixels --------------------------------------------
        $display("[P2] 16 WRSP: 4 cantos coloridos + linha branca no meio");
        mostrar = 6;
        wrsp(3,  0,  0, 8'hE0);        // sup-esq  vermelho
        wrsp(3, 15,  0, 8'h1C);        // sup-dir  verde
        wrsp(3,  0, 15, 8'hFC);        // inf-esq  amarelo
        wrsp(3, 15, 15, 8'h03);        // inf-dir  azul
        for (k = 2; k <= 13; k = k + 1) wrsp(3, k, 7, 8'hFF);   // linha branca y=7
        quadro;
        // calibra o deslocamento horizontal do pipeline (0 ou 1 pixel)
        d = 0;
        for (c = -2; c <= 2; c = c + 1)
            if (fb[SY * 320 + SX + c] === 8'hE0) d = c;
        $display("     (deslocamento do pipeline compositor/sprite: %0d px)", d);
        mostrar_sprite("P2: depois dos 16 WRSP");
        chk( 0,  0, 8'hE0, "canto sup-esq vermelho");
        chk(15,  0, 8'h1C, "canto sup-dir verde");
        chk( 0, 15, 8'hFC, "canto inf-esq amarelo");
        chk(15, 15, 8'h03, "canto inf-dir azul");
        chk( 2,  7, 8'hFF, "linha branca (inicio)");
        chk( 8,  7, 8'hFF, "linha branca (meio)");
        chk(13,  7, 8'hFF, "linha branca (fim)");

        // ---- P3: reescrever (dinâmico) ----------------------------------------
        $display("[P3] reescrevendo pixels JA gravados: cantos mudam de cor, linha vira magenta, canto inf-dir vira TRANSPARENTE");
        mostrar = 4;
        wrsp(3,  0,  0, 8'h1F);        // vermelho -> ciano
        wrsp(3, 15,  0, 8'hFF);        // verde    -> branco
        wrsp(3, 15, 15, 8'h00);        // azul     -> transparente
        for (k = 2; k <= 13; k = k + 1) wrsp(3, k, 7, 8'hE3);   // branca -> magenta
        quadro;
        mostrar_sprite("P3: depois de sobrescrever");
        chk( 0,  0, 8'h1F, "vermelho virou ciano");
        chk(15,  0, 8'hFF, "verde virou branco");
        chk_transp(15, 15, "azul virou transparente");
        chk( 0, 15, 8'hFC, "amarelo NAO mudou");
        chk( 2,  7, 8'hE3, "linha virou magenta (ini)");
        chk(13,  7, 8'hE3, "linha virou magenta (fim)");

        // ---- P4: isolamento entre slots ----------------------------------------
        $display("[P4] escreve no slot 5 (que nenhum sprite mostra): o sprite do slot 3 NAO pode mudar");
        mostrar = 2;
        wrsp(5, 0, 0, 8'hFF);
        wrsp(5, 1, 1, 8'hE3);
        wrsp(5, 15, 0, 8'hFC);
        quadro;
        mostrar_sprite("P4: slot 3 depois de escrever no slot 5");
        chk( 0,  0, 8'h1F, "slot 3 intacto (0,0)");
        chk( 0, 15, 8'hFC, "slot 3 intacto (0,15)");
        chk( 8,  7, 8'hE3, "slot 3 intacto (8,7)");
        $display("     agora o sprite 1 aponta para o slot 5 em (100,60)");
        enviar(i_rgsp(1, 5));
        enviar(i_possp(1, 100, 60));
        enviar(i_atsp(1, 0, 0, 1));
        quadro;
        if (fb[60 * 320 + 100 + d] === 8'hFF) begin acertos = acertos + 1; $display("   ok     slot 5 (0,0) = 0xFF  no sprite 1"); end
        else begin erros = erros + 1; $display("   FALHOU slot 5 (0,0): leu 0x%02h, esperado 0xFF", fb[60 * 320 + 100 + d]); end
        if (fb[61 * 320 + 101 + d] === 8'hE3) begin acertos = acertos + 1; $display("   ok     slot 5 (1,1) = 0xE3  no sprite 1"); end
        else begin erros = erros + 1; $display("   FALHOU slot 5 (1,1): leu 0x%02h, esperado 0xE3", fb[61 * 320 + 101 + d]); end
        if (fb[60 * 320 + 115 + d] === 8'hFC) begin acertos = acertos + 1; $display("   ok     slot 5 (15,0) = 0xFC no sprite 1"); end
        else begin erros = erros + 1; $display("   FALHOU slot 5 (15,0): leu 0x%02h, esperado 0xFC", fb[60 * 320 + 115 + d]); end

        // ---- P5: animação de 1 pixel -------------------------------------------
        $display("[P5] o mesmo pixel (8,3) do slot 3 regravado 4 vezes, uma cor por quadro");
        for (c = 0; c < 4; c = c + 1) begin
            case (c)
                0: wrsp(3, 8, 3, 8'hE0);
                1: wrsp(3, 8, 3, 8'h1C);
                2: wrsp(3, 8, 3, 8'h03);
                default: wrsp(3, 8, 3, 8'hFC);
            endcase
            quadro;
            case (c)
                0: chk(8, 3, 8'hE0, "quadro 1: vermelho");
                1: chk(8, 3, 8'h1C, "quadro 2: verde");
                2: chk(8, 3, 8'h03, "quadro 3: azul");
                default: chk(8, 3, 8'hFC, "quadro 4: amarelo");
            endcase
        end

        // ---- resumo -----------------------------------------------------------
        $display("");
        $display("instrucoes enviadas pelo handshake: %0d", ninst);
        $display("ciclos em que MotorSprites.dados != dados_inst (ligacao errada): %0d", erros_ligacao);
        if (erros_ligacao != 0)
            $display(">>> CAUSA: a porta .dados do MotorSprites NAO esta ligada em main.v (veja [UC] x MotorSprites.dados acima).");
        $display("checagens ok: %0d | falhas: %0d", acertos, erros);
        if (erros == 0 && erros_ligacao == 0) $display("RESULTADO: PASSOU - o WRSP escreve e altera os pixels da RAM de sprites");
        else                                  $display("RESULTADO: REPROVOU");
        $display("================================================");
        $finish;
    end

    // proteção contra travamento da simulação
    initial begin
        #400_000_000;
        $display("*** ABORTADO: simulacao passou de 400 ms");
        $finish;
    end
endmodule