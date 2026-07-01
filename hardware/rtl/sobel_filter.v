// sobel_filter.v

/* verilator lint_off WIDTHEXPAND */
/* verilator lint_off WIDTHTRUNC */
module sobel_filter #(
    parameter int WIDTH = 640,
    parameter int DATA_WIDTH = 8
) (
    input clk,
    input rst_n,
    input in_valid,
    input [DATA_WIDTH-1:0] pixel_in,
    output reg [DATA_WIDTH-1:0] pixel_out,
    output reg out_valid
);
  wire [DATA_WIDTH-1:0] lb1_pixel;
  wire [DATA_WIDTH-1:0] lb2_pixel;

  // 1. Data Path Line Buffers
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

  // 2. Spatial Matrix Window Registers
  reg [DATA_WIDTH-1:0] p00, p01, p02;
  reg [DATA_WIDTH-1:0] p10, p11, p12;
  reg [DATA_WIDTH-1:0] p20, p21, p22;

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      {p00, p01, p02, p10, p11, p12, p20, p21, p22} <= 0;
    end else if (in_valid) begin
      // Horizontal slide window shift
      p00 <= p01; p01 <= p02; p02 <= lb2_pixel; // Row 0 (Oldest)
      p10 <= p11; p11 <= p12; p12 <= lb1_pixel; // Row 1
      p20 <= p21; p21 <= p22; p22 <= pixel_in;  // Row 2 (Current)
    end
  end

  // 3. High-Precision Timing & Pipeline Alignment Control
  // Latency = 2 full rows (WIDTH * 2) + 2 horizontal cycles to center the 3x3 matrix.
  reg [11:0] startup_counter;
  reg frame_primed;

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      startup_counter <= 0;
      frame_primed    <= 1'b0;
      out_valid       <= 1'b0;
    end else if (in_valid) begin
      if (!frame_primed) begin
        if (startup_counter == 12'((WIDTH * 2) + 2)) begin
          frame_primed <= 1'b1;
        end else begin
          startup_counter <= startup_counter + 1'b1;
        end
      end
      // Match the 1-clock-cycle registration latency of pixel_out
      out_valid <= frame_primed;
    end else begin
      // Maintain continuous validation stream while simulation flushes trailing pixels
      out_valid <= frame_primed && (startup_counter >= 12'((WIDTH * 2) + 2));
    end
  end

  // 4. Combinational Edge Convolution Arithmetic
  reg signed [10:0] Gx;
  reg signed [10:0] Gy;
  reg signed [10:0] abs_Gx;
  reg signed [10:0] abs_Gy;
  reg signed [10:0] sum;
  reg [7:0] clamped_sum;

  always @(*) begin
    // Apply horizontal and vertical Sobel kernel masks
    Gx = (p02 + (p12 << 1) + p22) - (p00 + (p10 << 1) + p20);
    Gy = (p20 + (p21 << 1) + p22) - (p00 + (p01 << 1) + p02);

    if (Gx < 0) abs_Gx = -Gx;
    else        abs_Gx = Gx;

    if (Gy < 0) abs_Gy = -Gy;
    else        abs_Gy = Gy;

    sum = abs_Gx + abs_Gy;

    if (sum > 255) clamped_sum = 8'hFF;
    else           clamped_sum = sum[7:0];
  end

  // 5. Output Stage Registration
  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pixel_out <= 0;
    end else begin
      // Always capture the math so trailing stream segments can flush out of the core safely
      pixel_out <= clamped_sum;
    end
  end

endmodule
