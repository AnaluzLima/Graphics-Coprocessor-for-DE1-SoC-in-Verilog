module FrameBuffers (
    input clk,
    input vsync,
    
    //conexão com os motores
    input [16:0] write_addr,
    input [7:0]  write_data,
    input write_en,
    
    //conexão com o monitor
    input [16:0] read_addr,
    output [7:0] read_data
);

    //controle para seleção do buffer
    reg buffer_sel;
    always @(negedge vsync) begin
        buffer_sel <= ~buffer_sel;
    end
    
    //demux de escrita
    wire we_A = (buffer_sel == 1'b0) ? write_en : 1'b0;
    wire we_B = (buffer_sel == 1'b1) ? write_en : 1'b0;
    
    wire [7:0] out_A, out_B;
    
    //mux de leitura
    assign read_data = (buffer_sel == 1'b0) ? out_B : out_A;

    //memória
    ram_buffer buffer_A (
        .clock (clk),
        .data (write_data),
        .wraddress (write_addr), //escrita do motor
        .wren (we_A),
        .rdaddress (read_addr), //leitura do vga
        .q (out_A)
    );
    
    ram_buffer buffer_B (
        .clock (clk),
        .data (write_data),
        .wraddress (write_addr),
        .wren (we_B),
        .rdaddress (read_addr),
        .q (out_B)
    );

endmodule