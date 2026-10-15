module rasterizadorPoligonos (
	input clk,

	input [8:0] next_x,
	input [7:0] next_y,

	input [799:0] poligono,

	output reg [7:0] draw
);

	//pertinência de Pixel para os 8 Polígonos
	wire signed [11:0] px = {3'b000, next_x};
	wire signed [11:0] py = {4'b0000, next_y};

	wire [15:0] dentro;

	genvar i;
	generate
	  for (i = 0; i < 16; i = i + 1) begin : check_poly

	  	wire [49:0] poly = poligono [i*50 +: 50];

		wire [8:0] Ax_raw = poly[8:0];
		wire [7:0] Ay_raw = poly[16:9];
		wire [8:0] width_raw = poly[25:17];
      wire [7:0] height_raw = poly[33:26];
		wire show_raw = poly[42];
		wire [2:0] tipo_raw = poly[45:43];
					
		wire signed [11:0] ax = {3'b000, Ax_raw};
		wire signed [11:0] ay = {4'b0000, Ay_raw};
		wire signed [11:0] w  = {3'b000, width_raw};
      wire signed [11:0] h  = {4'b0000, height_raw};

		//vértices B e C calculados
		reg signed [11:0] bx, by, cx, cy;
		always @(*) begin
			case (tipo_raw)
				3'd2: begin // Equilátero
					bx = ax - (w >>> 1); by = ay + h;
                    cx = ax + (w >>> 1); cy = ay + h;
				end
				3'd3: begin // Isósceles
					bx = ax - (w >>> 2); by = ay + h;
                    cx = ax + (w >>> 2); cy = ay + h;
				end
				3'd4: begin // Escaleno
					bx = ax; by = ay + h;
                    cx = ax + w; cy = ay + h;
				end
				default: begin
					bx = ax; by = ay;
					cx = ax; cy = ay;
				end
			endcase
		end
					
		//equações de borda para triângulos
		wire signed [23:0] e1 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
		wire signed [23:0] e2 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
		wire signed [23:0] e3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);

		wire pixel_no_ret = (px >= ax) && (px <= ax + w) && (py >= ay) && (py <= ay + h);
        wire pixel_no_qua = (px >= ax) && (px <= ax + w) && (py >= ay) && (py <= ay + w);
        wire pixel_no_tri = ((e1 <= 0) && (e2 <= 0) && (e3 <= 0)) || ((e1 >= 0) && (e2 >= 0) && (e3 >= 0));

		assign dentro[i] = show_raw && ((tipo_raw == 3'd0 && pixel_no_ret) || (tipo_raw == 3'd1 && pixel_no_qua) || 
		(tipo_raw >= 3'd2 && tipo_raw <= 3'd4 && pixel_no_tri));
	end
endgenerate

	//codificador de Prioridade e Saída
	reg [3:0] poly_sel;
	reg hit;
	integer j;
	always @(*) begin
		hit = 1'b0;
		poly_sel = 4'd0;
		for (j = 15; j >= 0; j = j - 1) begin
			if (dentro[j]) begin
					poly_sel = j[3:0];
					hit = 1'b1;
			end
		end
	end

	reg hit_reg;
	reg [7:0] cor_reg;
	always @(posedge clk) begin
		hit_reg <= hit;
		cor_reg <= poligono[poly_sel*50 + 34 +: 8];
	end

	always @(*) begin
		if (hit_reg && (cor_reg != 8'd0))
			draw = cor_reg;
		else
			draw = 8'd0;
	end

endmodule

module RegPoligonos (
	input clk, rst, 
	input [2:0] pol_en, 
	input [26:0] dados,
	
	output [799:0] poligono
);

	reg [49:0] banco [0:15];
	// Registrador temporário para permitir modificação de fatias
    reg [49:0] temp_poly;
	reg [3:0] id;

	integer i;
	always @(posedge clk) begin
		if (rst) begin
			banco[0] <= {4'd0, 3'd0, 1'b0, 8'hE0, 8'd15, 9'd40, 8'd30,  9'd30}; // Retângulo
            banco[1] <= {4'd1, 3'd1, 1'b0, 8'h1C, 8'd30, 9'd30, 8'd30,  9'd100}; // Quadrado
            banco[2] <= {4'd2, 3'd2, 1'b0, 8'h03, 8'd35, 9'd40, 8'd30,  9'd170}; // Equilátero
            banco[3] <= {4'd3, 3'd3, 1'b0, 8'hFC, 8'd45, 9'd20, 8'd30,  9'd240}; // Isósceles
            banco[4] <= {4'd4, 3'd4, 1'b0, 8'hE3, 8'd30, 9'd40, 8'd120, 9'd50}; // Escaleno
			for (i = 5; i<=15; i = i+1) begin
				banco[i] <= 50'd0;
			end

		end
		else begin
			case (pol_en)
                3'b001: begin //POSPO
						  id = dados[20:17];
						  temp_poly = banco[id];
                    temp_poly[16:9] = dados[7:0]; // Y
                    temp_poly[8:0] = dados[16:8]; // X
						  banco[id] <= temp_poly;
                end
                3'b010: begin //ATPO
						  id = dados[7:4];
						  temp_poly = banco[id];
                    temp_poly[45:43] = dados[2:0]; //tipo
                    temp_poly[42] = dados[3]; //show
						  banco[id] <= temp_poly;
                end
                3'b011: begin //TAMPO
						id = dados[20:17];
						temp_poly = banco[id];
						temp_poly[33:26] = dados[7:0]; //altura
						temp_poly[25:17] = dados[16:8]; //largura
						banco[id] <= temp_poly;
                end
					 3'b100: begin //CORPO
						id = dados[11:8];
						temp_poly = banco[id];
						temp_poly[41:34] = dados[7:0]; //cor
						banco[id] <= temp_poly;
                end
            endcase
		end
	end

	genvar k;
	generate
		for (k=0; k<16; k=k+1) begin: empacotar
			assign poligono[k*50 +: 50] = banco[k];
		end
	endgenerate

endmodule