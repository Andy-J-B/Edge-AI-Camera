module line_buffer ( 
  input clk, rst_n,
  input in_valid,
  input [7:0] pixel_in,
  output reg[7:0] pixel_out
);
  
  parameter WIDTH = 640;
  parameter DATA_WIDTH = 8;

endmodule

