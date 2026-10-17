module enderecoVGA (
	input [9:0] vga_raw_x, vga_raw_y,
	output [16:0] enderecoL
);

	wire [9:0] teste = (vga_raw_x == 10'd639) ? 10'd639 : (vga_raw_x + 10'd1);
	wire [8:0] log_vga_x = teste[9:1];
	wire [7:0] log_vga_y = vga_raw_y[9:1];
	assign enderecoL = (log_vga_y * 17'd320) + log_vga_x;
	
endmodule