module main(
    input [31:0] cmd_in,

    input reset, clk, clk25, vsync, 

    input [9:0] vga_raw_x, vga_raw_y,

    input valid_FIFO,
    output busy_FIFO,
    output done_FIFO,

    output [7:0] corFinal
);
        
    wire [31:0] fifo_inst_out;
    wire fifo_empty, fifo_full, rd_enable, uc_exec_pop;

    wire [1:0] sprite_en;
    wire sprite_ram_en;
    wire bg_en;
    wire bg_ram_en;
    wire [2:0] pol_en;
    wire [26:0] dados_inst;
    wire prioridade_uc;

    wire [959:0] sprites_bus;
    wire [799:0] poligono_bus;

    wire [8:0] scan_x;
    wire [7:0] scan_y;

    wire [7:0] cor_bg, cor_sprites, cor_poly;

    wire [9:0] teste = (vga_raw_x == 10'd639) ? 10'd639 : (vga_raw_x + 10'd1);
	 wire [8:0] log_vga_x = teste[9:1];
    wire [7:0] log_vga_y = vga_raw_y[9:1];
    wire [16:0] enderecoL = (log_vga_y * 17'd320) + log_vga_x;

    wire [16:0] fb_write_addr;
    wire [7:0] fb_write_data;
    wire fb_write_en;

    fifo fifo (
        .clk (clk),
        .rst (reset),
        .instr_in (cmd_in),
        .exec_pop (uc_exec_pop),
        .valid (valid_FIFO),
        .busy (busy_FIFO),
        .done (done_FIFO),
        .instr_out (fifo_inst_out),
        .fifo_empty(fifo_empty),
        .fifo_full (fifo_full)
    );

    UnidadeControle uc (
        .clk (clk),
        .rst (reset),
        .fifo_empty (fifo_empty),
        .fifo_full (fifo_full),
        .fifo_inst (fifo_inst_out),
        .exec_pop (uc_exec_pop),
        .sprite_en (sprite_en),
        .sprite_ram_en (sprite_ram_en),
        .pol_en (pol_en),
        .bg_en (bg_en),
        .bg_ram_en (bg_ram_en),
        .dados(dados_inst),
        .vsync (vsync),
        .prioridade (prioridade_uc)
    );

    RegSprites regS( 
        .clk(clk), 
        .rst(reset), 
        .sprite_en(sprite_en), 
        .dados(dados_inst), 
        .sprite (sprites_bus)
    );

    RegPoligonos regP (
        .clk (clk),
        .rst (reset),
        .pol_en (pol_en),
        .dados (dados_inst),
        .poligono (poligono_bus)
    );

    MotorBG bg (
        .clk (clk),
        .rst (reset),
        .vsync (vsync),
        .bg_x (scan_x),
        .bg_y (scan_y),
        .qual_op ({bg_ram_en, bg_en}),
        .dados (dados_inst),
        .draw (cor_bg)
    );

    MotorSprites sp (
        .clk (clk),
        .rst (reset),
        .next_x (scan_x),
        .next_y (scan_y),
        .sprites (sprites_bus),
        .qual_op (sprite_ram_en),
		  .dados (dados_inst),
        .draw (cor_sprites)
    );

    rasterizadorPoligonos polig (
        .clk (clk25),
        .next_x (scan_x),
        .next_y (scan_y),
        .poligono (poligono_bus),
        .draw (cor_poly)
    );


    Compositor comp (
        .clk (clk25),
        .rst (reset),
        .draw_bg (cor_bg),
        .draw_sprites  (cor_sprites),
        .draw_pol (cor_poly),
        .prioridade (prioridade_uc),
        .scan_x (scan_x),
        .scan_y (scan_y),
        .fb_write_addr (fb_write_addr),
        .fb_write_data (fb_write_data),
        .fb_write_en (fb_write_en)
    );

    FrameBuffers buffers (
        .clk (clk),
        .vsync (vsync),
        .write_addr (fb_write_addr),
        .write_data (fb_write_data),
        .write_en (fb_write_en),
        .read_addr (enderecoL),
        .read_data (corFinal)
    );

endmodule