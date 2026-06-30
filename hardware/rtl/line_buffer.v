// line_buffer.v
module line_buffer #(
    parameter int WIDTH = 640,
    parameter int DATA_WIDTH = 8
) (
    input clk,
    rst_n,
    input in_valid,
    input [DATA_WIDTH-1:0] pixel_in,
    output reg [DATA_WIDTH-1:0] pixel_out
);
  reg [$clog2(WIDTH)-1:0] pointer;

  reg [DATA_WIDTH-1:0] line_buffer[WIDTH];

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) pointer <= 0;
    else if (in_valid)
      if (pointer == 10'(WIDTH - 1)) pointer <= 0;
      else pointer <= pointer + 1;
  end
  always @(posedge clk) begin
    if (in_valid) begin
      pixel_out <= line_buffer[pointer];
      line_buffer[pointer] <= pixel_in;
    end
  end
endmodule

