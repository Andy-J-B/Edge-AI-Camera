// sobel_filter.v

module sobel_filter #(
    parameter int WIDTH = 640,
    parameter int DATA_WIDTH = 8
) (
    input clk,
    res_n,
    input in_valid,
    input [7:0] pixel_in,
    output [7:0] pixel_out
);

endmodule
