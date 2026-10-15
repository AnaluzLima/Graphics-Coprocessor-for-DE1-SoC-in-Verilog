module MotorBG (
	input clk, rst, vsync,
	input [8:0] bg_x, //coordenada X
	input [7:0] bg_y, //coordenada Y

	input [1:0] qual_op,
	input [26:0] dados,
	
	output [7:0] draw
);

	reg wr_en;
	reg [10:0] wr_addr;
	reg [7:0] wr_data;

	reg [5:0] scroll_x; //deslocamento horizontal 
	reg [4:0] scroll_y; //deslocamento vertical

	always @(posedge clk or posedge rst) begin
		if (rst) begin
			wr_en    <= 1'b0;
			wr_addr  <= 11'b0;
			wr_data  <= 8'b0;
			scroll_x <= 6'b0;
			scroll_y <= 5'b0;
		end else begin
			wr_en <= qual_op[1];                 
			if (qual_op[1]) begin
				wr_addr <= dados[18:8];
				wr_data <= dados[7:0];
			end
			if (qual_op[0]) begin                
				scroll_x <= dados[10:5];
				scroll_y <= dados[4:0];
			end
		end
	end

	reg [5:0] scroll_x_atrasado;
    reg [4:0] scroll_y_atrasado;

    always @(negedge vsync or posedge rst) begin
        if (rst) begin
            scroll_x_atrasado <= 6'd0;
            scroll_y_atrasado <= 5'd0;
        end else begin
            scroll_x_atrasado <= scroll_x;
            scroll_y_atrasado <= scroll_y;
        end
    end

	wire [5:0] col_pixel = bg_x[8:3];
	wire [4:0] lin_pixel = bg_y[7:3];

	wire [6:0] soma_col = col_pixel + scroll_x_atrasado;
	wire [5:0] col_final = (soma_col >= 7'd40) ? (soma_col - 7'd40) : soma_col[5:0];

	wire [5:0] soma_lin = lin_pixel + scroll_y_atrasado;
	wire [4:0] lin_final = (soma_lin >= 6'd30) ? (soma_lin - 6'd30) : soma_lin[4:0];

	//endereço na grade de 1200 posições
	wire [10:0] tilemap_addr = (lin_final * 6'd40) + col_final;

	ram_tilemap tm (
        .clock (clk),
        .data (wr_data),
        .wraddress (wr_addr),
        .wren (wr_en),
        .rdaddress (tilemap_addr),
        .q (draw)
    );

endmodule
