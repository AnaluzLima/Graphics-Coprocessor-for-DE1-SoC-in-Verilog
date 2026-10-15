module MotorSprites (
	input clk, rst,

	input [8:0] next_x,
	input [7:0] next_y,
	
	//atributos
	input [959:0] sprites,

	input qual_op,

	input [26:0] dados,

	output reg [7:0] draw
);

	reg [12:0] wr_addr;
	reg [7:0] wr_data;
	reg wr_en;

	always @(posedge clk or posedge rst) begin
    if (rst) begin
        wr_en   <= 1'b0;
        wr_addr <= 13'b0;
        wr_data <= 8'b0;
    end else begin
        if (qual_op) begin // quando sprite_ram_en estiver ativo
            wr_en   <= 1'b1;
            wr_addr <= dados[20:8];
            wr_data <= dados[7:0];
        end else begin
            wr_en   <= 1'b0;
        end
    end
	end


	//verifica bbox para colisao
	wire [31:0] colisao;
	genvar i;
	generate
		for (i = 0; i < 32; i = i + 1) begin : check_box

			wire [31:0] sp = sprites[i*30 +: 30];

			wire [8:0] sx = sp[8:0];
			wire [7:0] sy = sp[16:9];
			wire show = sp[17];
			
			assign colisao[i] = show && (next_x >= sx) && (next_x < sx + 9'd16) && (next_y >= sy) && (next_y < sy + 8'd16);
		end
	endgenerate

   //sprite 0 tem a maior prioridade
    reg [4:0] sprite_sel;
    reg hit;
    integer j;
    always @(*) begin
        hit = 1'b0;
        sprite_sel = 5'd0;
        for (j = 31; j >= 0; j = j - 1) begin
            if (colisao[j]) begin
                sprite_sel = j[4:0];
                hit = 1'b1;
            end
        end
    end
	
	//Atributos do sprite selecionado
	wire [31:0] sel_sp = sprites[sprite_sel*30 +: 30];
	wire [8:0] sel_sx = sel_sp[8:0];
	wire [7:0] sel_sy = sel_sp[16:9];
	wire sel_flip_h = sel_sp[18];
	wire sel_flip_v = sel_sp[19];
	wire [4:0] sel_spriteram = sel_sp[24:20];

	//calculo do Offset do Pixel Interno com Flip
	wire [3:0] raw_dx = next_x - sel_sx;
	wire [3:0] raw_dy = next_y - sel_sy;

	wire [3:0] final_dx = sel_flip_h ? (4'd15 - raw_dx) : raw_dx;
	wire [3:0] final_dy = sel_flip_v ? (4'd15 - raw_dy) : raw_dy;

	//enderecamento da Memoria (8192 palavras: ID * 256 + dy * 16 + dx)
	wire [12:0] ram_addr = {sel_spriteram, final_dy, final_dx};
	wire [7:0]  cor_pixel_ram;
	
	ram_sprites sprites_mem (
        .clock (clk),
        .data (wr_data),
        .wraddress (wr_addr),
        .wren (wr_en),
        .rdaddress (ram_addr),
        .q (cor_pixel_ram)
    );

	//sincronizacao de 1 ciclo para a saida
	reg hit_reg;
	always @(posedge clk) begin
	  hit_reg <= hit;
	end

	//selecao final da cor (Transparencia no indice 0) --- desenho
	always @(*) begin
	  if (hit_reg && (cor_pixel_ram != 8'd0))
			draw = cor_pixel_ram;
	  else
			draw = 8'd0;
	end

endmodule

module RegSprites (
	input clk, rst, 
	input [1:0] sprite_en, 
	input [26:0] dados,
	
	output [959:0] sprite
);

	reg [29:0] banco [0:31];
	reg [29:0] temp_sp;
	reg [4:0] id;

	integer i;
	always @(posedge clk) begin
		if (rst) begin
			for (i = 0; i < 32; i = i+1) begin
				banco[i] <= 30'd0;
			end

		end
		else begin
			case (sprite_en)
				2'b01: begin //POSSP
					id = dados[21:17];
					temp_sp = banco[id];
					temp_sp[16:9] = dados[7:0]; //Y
					temp_sp[8:0] = dados[16:8]; //X
					banco[id] <= temp_sp;
				end
				2'b10: begin //ATSP
					id = dados[7:3];
					temp_sp = banco[id];
					temp_sp[19:18] = dados[2:1]; //flip
					temp_sp[17] = dados[0]; //show
					banco[id] <= temp_sp;
				end
				2'b11: begin //RGSP
					id = dados[9:5];
					temp_sp = banco[id];
					temp_sp[24:20] = dados[4:0]; //id da ram
					banco[id] <= temp_sp;
				end

			endcase
		end
	end

	genvar k;
	generate
		for (k=0; k < 32; k=k+1) begin: empacotar
			assign sprite[k*30 +: 30] = banco[k];
		end
	endgenerate

endmodule