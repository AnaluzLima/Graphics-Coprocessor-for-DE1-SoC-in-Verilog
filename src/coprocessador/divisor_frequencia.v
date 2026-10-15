module divisor_frequencia (
	input E,
	input [4:0] dividido,
	output S
);
	reg [25:0] clk;
	
	always @(posedge E) begin
		clk[0] <= ~clk[0];
	end
	always @(posedge clk[0]) begin
		clk[1] <= ~clk[1];
	end
	always @(posedge clk[1]) begin
		clk[2] <= ~clk[2];
	end
	always @(posedge clk[2]) begin
		clk[3] <= ~clk[3];
	end
	always @(posedge clk[3]) begin
		clk[4] <= ~clk[4];
	end
	always @(posedge clk[4]) begin
		clk[5] <= ~clk[5];
	end
	always @(posedge clk[5]) begin
		clk[6] <= ~clk[6];
	end
	always @(posedge clk[6]) begin
		clk[7] <= ~clk[7];
	end
	always @(posedge clk[7]) begin
		clk[8] <= ~clk[8];
	end
	always @(posedge clk[8]) begin
		clk[9] <= ~clk[9];
	end
	always @(posedge clk[9]) begin
		clk[10] <= ~clk[10];
	end
	always @(posedge clk[10]) begin
		clk[11] <= ~clk[11];
	end
	always @(posedge clk[11]) begin
		clk[12] <= ~clk[12];
	end
	always @(posedge clk[12]) begin
		clk[13] <= ~clk[13];
	end
	always @(posedge clk[13]) begin
		clk[14] <= ~clk[14];
	end
	always @(posedge clk[14]) begin
		clk[15] <= ~clk[15];
	end
	always @(posedge clk[15]) begin
		clk[16] <= ~clk[16];
	end
	always @(posedge clk[16]) begin
		clk[17] <= ~clk[17];
	end
	always @(posedge clk[17]) begin
		clk[18] <= ~clk[18];
	end
	always @(posedge clk[18]) begin
		clk[19] <= ~clk[19];
	end
	always @(posedge clk[19]) begin
		clk[20] <= ~clk[20];
	end
	always @(posedge clk[20]) begin
		clk[21] <= ~clk[21];
	end
	always @(posedge clk[21]) begin
		clk[22] <= ~clk[22];
	end
	always @(posedge clk[22]) begin
		clk[23] <= ~clk[23];
	end
	always @(posedge clk[23]) begin
		clk[24] <= ~clk[24];
	end
		
	assign S = clk[dividido-1];
	
endmodule