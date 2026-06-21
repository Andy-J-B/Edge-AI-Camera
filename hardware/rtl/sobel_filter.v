// sobel_filter.v

module sobel_filter #(
    parameter int WIDTH = 640,
    parameter int DATA_WIDTH = 8
) (
    input clk,
    rst_n,
    input in_valid,
    input [DATA_WIDTH-1:0] pixel_in,
    output reg [DATA_WIDTH-1:0] pixel_out,
    output reg out_valid
);
  wire [DATA_WIDTH-1:0] lb1_pixel;
  wire [DATA_WIDTH-1:0] lb2_pixel;
  wire lb1_valid;
  wire lb2_valid;

  line_buffer #(
      .WIDTH(WIDTH),
      .DATA_WIDTH(DATA_WIDTH)
  ) line_buffer1 (
      .clk(clk),
      .rst_n(rst_n),
      .in_valid(in_valid),
      .pixel_in(pixel_in),
      .pixel_out(lb1_pixel)
  );

  line_buffer #(
      .WIDTH(WIDTH),
      .DATA_WIDTH(DATA_WIDTH)
  ) line_buffer2 (
      .clk(clk),
      .rst_n(rst_n),
      .in_valid(in_valid),
      .pixel_in(lb1_pixel),
      .pixel_out(lb2_pixel)
  );

  line_buffer #(
      .WIDTH(WIDTH),
      .DATA_WIDTH(1)
  ) control_pipeline1 (
      .clk(clk),
      .rst_n(rst_n),
      .in_valid(in_valid),
      .pixel_in(in_valid),
      .pixel_out(lb1_valid)
  );

  line_buffer #(
      .WIDTH(WIDTH),
      .DATA_WIDTH(1)
  ) control_pipeline2 (
      .clk(clk),
      .rst_n(rst_n),
      .in_valid(in_valid),
      .pixel_in(lb1_valid),
      .pixel_out(lb2_valid)
  );

endmodule
