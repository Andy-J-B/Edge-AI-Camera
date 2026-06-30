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

  reg [DATA_WIDTH-1:0] p00, p01, p02;
  reg [DATA_WIDTH-1:0] p10, p11, p12;
  reg [DATA_WIDTH-1:0] p20, p21, p22;

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      {p00, p01, p02, p10, p11, p12, p20, p21, p22} <= {9{1'b0}};
    end else if (in_valid) begin
      // horizontal shift
      p00 <= p01;
      p01 <= p02;
      p02 <= lb2_pixel;
      p10 <= p11;
      p11 <= p12;
      p12 <= lb1_pixel;
      p20 <= p21;
      p21 <= p22;
      p22 <= pixel_in;
    end
  end

  reg valid_delay1;

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      valid_delay1 <= 1'b0;
      out_valid <= 1'b0;
    end else if (in_valid) begin
      valid_delay1 <= lb2_valid;
      out_valid <= valid_delay1;
    end
  end

  reg signed [10:0] Gx;
  reg signed [10:0] Gy;
  reg signed [10:0] abs_Gx;
  reg signed [10:0] abs_Gy;
  reg signed [10:0] sum;

  // Horizontal and vertical kernels
  always @(posedge clk or negedge rst_n) begin
    Gx <= (p02 + (p12 << 1) + p22) - (p00 + (p10 << 1) + p20);
    Gy <= (p20 + (p21 << 1) + p22) - (p00 + (p01 << 1) + p02);

    // abs
    abs_Gx <= Gx;
    abs_Gy <= Gy;
    if (abs_Gx[10] == 1) abs_Gx[10] <= 0;
    if (abs_Gy[10] == 1) abs_Gy[10] <= 0;
    sum <= abs_Gx + abs_Gy;

    if (sum > 255) sum <= 255;

    if (in_valid) pixel_out <= sum;
  end

endmodule
