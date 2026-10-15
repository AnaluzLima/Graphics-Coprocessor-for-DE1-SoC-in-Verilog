module Compositor (
	input clk, rst,

	input [7:0] draw_bg,
	input [7:0] draw_sprites,
	input [7:0] draw_pol,

	input prioridade,
	
	output [8:0] scan_x,
	output [7:0] scan_y,

	output reg [16:0] fb_write_addr,
	output reg [7:0] fb_write_data,
	output reg fb_write_en
);

	//varredor virtual
	reg [8:0] count_x;
	reg [7:0] count_y;

	always @(posedge clk or posedge rst) begin
	  if (rst) begin
			count_x <= 9'd0;
			count_y <= 8'd0;
	  end else begin
			if (count_x == 9'd319) begin
				 count_x <= 9'd0;
				 if (count_y == 8'd239)
					  count_y <= 8'd0;
				 else
					  count_y <= count_y + 1'b1;
			end else begin
				 count_x <= count_x + 1'b1;
			end
	  end
	end

	assign scan_x = count_x;
	assign scan_y = count_y;

	//endereço no FrameBuffer (Y * 320 + X)
	wire [16:0] current_addr = (count_y * 17'd320) + count_x;

	reg [7:0] sel_cor;

	always @(*) begin
	  if (!prioridade) begin
			if (draw_sprites != 8'd0) sel_cor = draw_sprites;
			else if (draw_pol != 8'd0) sel_cor = draw_pol;
			else sel_cor = draw_bg;
	  end else begin
			if (draw_pol != 8'd0) sel_cor = draw_pol;
			else if (draw_sprites != 8'd0) sel_cor = draw_sprites;
			else sel_cor = draw_bg;
	  end
	end

	always @(posedge clk or posedge rst) begin
	  if (rst) begin
			fb_write_addr <= 17'd0;
			fb_write_data <= 8'd0;
			fb_write_en <= 1'b0;
	  end else begin
			fb_write_addr <= current_addr;
			fb_write_data <= sel_cor;
			fb_write_en <= 1'b1;
	  end
	end

endmodule