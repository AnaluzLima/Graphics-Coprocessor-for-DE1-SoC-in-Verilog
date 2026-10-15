module fifo (
	input clk, rst,

	input [31:0] instr_in,
	input exec_pop,

	input valid,
	output reg busy, 
	output reg done,
	
	output [31:0] instr_out,
	output fifo_empty,
	output fifo_full
);

	parameter HAVE_INST = 2'd0;
	parameter WRD_INST = 2'd1; // WRD = WRITED
	parameter WAIT = 2'd2;

	reg [1:0] state;
	reg wr_en;

	reg [31:0] fila [0:15];
	reg [3:0] sp;
	reg [3:0] exec;
	reg [4:0] count;

	assign fifo_empty = (count == 5'd0);
	assign fifo_full = (count == 5'd16);

	assign instr_out = fila[exec];

	always @(posedge clk or posedge rst) begin
		if (rst) begin
			state <= HAVE_INST;
			busy <= 1'b0;
			done <= 1'b0;
			wr_en <= 1'b0;
		end else begin
			wr_en <= 1'b0;
			
			case (state)
				HAVE_INST: begin
					done <= 1'b0;
					if (valid && !fifo_full) begin
						busy <= 1'b1;
						wr_en <= 1'b1;

						state <= WRD_INST;
					end else begin
						busy <= 1'b0;
					end
				end

				WRD_INST: begin
					busy <= 1'b0;
					done <= 1'b1;
					//no ciclo seguinte, a escrita já ocorreu na FIFO
					state <= WAIT;
				end

				WAIT: begin
					//fica travado aqui até o ARM reconhecer e abaixar o valid
					if (!valid) begin
						done <= 1'b0;
						state <= HAVE_INST;
					end
				end
			endcase
		end
	end

	always @(posedge clk or posedge rst) begin
		if (rst) begin
			sp <= 4'd0;
			exec <= 4'd0;
			count <= 5'd0;
		end else begin
			if (wr_en && !fifo_full) begin
				fila[sp] <= instr_in;
				sp <= sp + 4'd1;
			end				

			if (exec_pop && !fifo_empty) begin
				exec <= exec + 4'd1;
			end

			case ({wr_en && !fifo_full, exec_pop && !fifo_empty})
				2'b10: count <= count + 5'd1;
				2'b01: count <= count - 5'd1;
				default: count <= count; 
			endcase
		end
	end

endmodule