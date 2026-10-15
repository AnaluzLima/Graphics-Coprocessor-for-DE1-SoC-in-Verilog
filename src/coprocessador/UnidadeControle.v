module UnidadeControle (
	input clk, rst,

	//fifo
	input fifo_empty,
	input fifo_full,
	input [31:0] fifo_inst,	
	output exec_pop,
	
	//sprite	
	output reg [1:0] sprite_en,
	output reg sprite_ram_en,
	
	//poligonos
	output reg [2:0] pol_en,
	
	//background
	output reg bg_en,
	output reg bg_ram_en,
	
	//Dados da instrução
	output reg [26:0] dados,

	//sync de video
	input vsync,
	output reg prioridade
);

	//estados
	parameter INIT = 3'd0;
	parameter GET_INST = 3'd1;
	parameter EXEC = 3'd2;
	parameter WAIT = 3'd3;
	
	reg [2:0] state, next_state;
	reg [31:0] instrucao;

	assign exec_pop = (state == GET_INST);
	
	//fatiando bits do registrador de instruçao
	wire [4:0] opcode = instrucao[4:0];
	
	//borda de descida do vsync
	reg vsync_reg;
	always @(posedge clk) vsync_reg <= vsync;
	wire vsync_negedge = (vsync_reg && !vsync);


	always @(posedge clk or posedge rst) begin
		if (rst) begin
			state <= INIT;
			instrucao <= 32'd0;
		end else begin
			state <= next_state;
			if (state == GET_INST) instrucao <= fifo_inst;
		end
	end
	
	wire [4:0] op_atual = (state == GET_INST) ? fifo_inst[4:0] : instrucao[4:0];

	always @(*) begin
		next_state = state;
		case (state)
			INIT: begin
				if (!fifo_empty) next_state = GET_INST;
			end

			GET_INST: begin
				next_state = EXEC;
			end

			EXEC: begin
				if (opcode == 5'b00000) next_state = WAIT;
				else if(!fifo_empty) next_state = GET_INST;
				else next_state = INIT;
			end

			WAIT: begin
				if (vsync_negedge) begin
					if (!fifo_empty) next_state = GET_INST;
					else next_state = INIT;
				end
			end

			default: next_state = INIT;
		endcase
	end

	always @(posedge clk or posedge rst) begin
		if (rst) begin
			sprite_en <= 2'b0;
			sprite_ram_en <= 1'b0;

			pol_en <= 3'b0;

			bg_en <= 1'b0;
			bg_ram_en <= 1'b0;

			dados <= 27'b0;
			prioridade <= 1'b0;

		end else begin
			sprite_en <= 2'b0;
			sprite_ram_en <= 1'b0;

			pol_en <= 3'b0;

			bg_en <= 1'b0;
			bg_ram_en <= 1'b0;
			
			dados <= 27'b0;
			
			case (state)
				INIT: begin
				end

				GET_INST: begin
				end

				EXEC: begin
					dados <= instrucao[31:5];
					case (opcode)
						5'b00000: begin // NoOp
							sprite_en <= 2'b0;
							sprite_ram_en <= 1'b0;

							pol_en <= 2'b0;

							bg_en <= 1'b0;
							bg_ram_en <= 1'b0;
			
							dados <= 27'b0;
						end

						//Background - Motor
						5'b00001: begin // BSCROLL
							bg_en <= 1'b1;
						end

						//Background - Motor/Memória
						5'b00010: begin // BTILE
							bg_ram_en <= 1'b1;
						end

						//Sprites - Registradores
						5'b00011: begin // POSSP
							sprite_en <= 2'b01;
						end

						5'b00100: begin // ATSP
							sprite_en <= 2'b10;
						end

						//Sprites - Motor
						5'b00101: begin // WRSP
							sprite_ram_en <= 1'b1;
						end

						5'b00110: begin // RGSP
							sprite_en <= 2'b11;
						end

						5'b00111: begin // ATPO
							pol_en <= 3'b010;
						end

						//Polígonos
						5'b01000: begin // POSPO
							pol_en <= 3'b001;
						end

						5'b01001: begin //TAMPO
							pol_en <= 3'b011;
						end
						
						//Polígonos
						5'b01010: begin //CORPO
							pol_en <= 3'b100;
						end

						//Prioridade
						5'b01011: begin //PRIOR
							prioridade <= instrucao[5];
						end

						default: begin
						end
					endcase
				end

				WAIT: begin
				end
			endcase
		end
	end
endmodule