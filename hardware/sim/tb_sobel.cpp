// tb_sobel.cpp
// Copyright (c) 2026 Andy. All Rights Reserved.
//

#include "Vsobel_filter.h"
#include "verilated.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

// BMP Header structures (Standard 54-byte uncompressed layout)
#pragma pack(push, 1)
struct BMPHeader {
  uint16_t type{0x4D42}; // "BM"
  uint32_t size{0};      // File size
  uint16_t reserved1{0};
  uint16_t reserved2{0};
  uint32_t offset{54}; // Pixel data offset
};

struct BMPInfoHeader {
  uint32_t size{40}; // Info header size
  int32_t width{0};
  int32_t height{0};
  uint16_t planes{1};
  uint16_t bit_count{24};  // Saving as 24-bit RGB for broad compatibility
  uint32_t compression{0}; // Uncompressed
  uint32_t image_size{0};
  int32_t x_pixels_per_meter{0};
  int32_t y_pixels_per_meter{0};
  uint32_t colors_used{0};
  uint32_t colors_important{0};
};
#pragma pack(pop)

int main(int argc, char *argv[]) {
  // 1. Initialize Verilator Environment context
  Verilated::commandArgs(argc, argv);
  auto top = std::make_unique<Vsobel_filter>();

  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <input_grayscale.bmp> <output_edge.bmp>\n";
    return 1;
  }

  // 2. Open and parse Input Grayscale BMP file
  std::ifstream infile(argv[1], std::ios::binary);
  if (!infile) {
    std::cerr << "Error: Could not open input file: " << argv[1] << "\n";
    return 1;
  }

  BMPHeader header;
  BMPInfoHeader info_header;
  infile.read(reinterpret_cast<char *>(&header), sizeof(header));
  infile.read(reinterpret_cast<char *>(&info_header), sizeof(info_header));

  if (info_header.width != 640) {
    std::cerr << "Error: Hardware module is hardcoded for WIDTH=640. Input "
                 "image width is "
              << info_header.width << ".\n";
    return 1;
  }

  // Determine row padding requirements for standard BMP format (multiples of 4
  // bytes)
  int input_row_padded = (info_header.width * 3 + 3) & (~3);
  int output_row_padded = (info_header.width * 3 + 3) & (~3);
  int height = std::abs(info_header.height);

  // Read raw pixel data into memory matrix buffer
  std::vector<uint8_t> input_pixels(height * info_header.width);
  std::vector<uint8_t> row_buffer(input_row_padded);

  // BMP images store pixels upside down (bottom row first). Read sequentially.
  for (int y = height - 1; y >= 0; --y) {
    infile.seekg(header.offset + y * input_row_padded);
    infile.read(reinterpret_cast<char *>(row_buffer.data()), input_row_padded);
    for (int x = 0; x < info_header.width; ++x) {
      // Take the blue channel out of the 24-bit representation as gray
      // intensity
      input_pixels[y * info_header.width + x] = row_buffer[x * 3];
    }
  }
  infile.close();

  // 3. Prepare Simulation Output Memory Capture Allocation
  std::vector<uint8_t> output_pixels(height * info_header.width, 0);

  // 4. Global Simulation Environment Initialization Reset Sequence
  top->clk = 0;
  top->rst_n = 0;
  top->in_valid = 0;
  top->pixel_in = 0;

  // Toggle clock layer twice to clear state registers cleanly
  for (int i = 0; i < 4; ++i) {
    top->clk = !top->clk;
    top->eval();
  }
  top->rst_n = 1; // Release reset active low hold flag

  // 5. Run Pixel Conveyor Streaming Simulation Loop
  std::cout << "Streaming " << input_pixels.size()
            << " pixels through Sobel Core...\n";

  size_t out_pixel_count = 0;
  size_t current_pixel_idx = 0;
  uint32_t total_cycles = 0;

  // Continue simulating until the hardware processing core has completely
  // flushed out the output frame matching the input image pixel count.
  while (out_pixel_count < input_pixels.size()) {
    top->clk = 1; // Clock Rising Edge Trigger Point

    if (current_pixel_idx < input_pixels.size()) {
      top->in_valid = 1;
      top->pixel_in = input_pixels[current_pixel_idx++];
    } else {
      // End of input stream reached; drive zero pad tail inputs
      top->in_valid = 0;
      top->pixel_in = 0;
    }

    top->eval(); // Compute sequential combinational logic propagation matrix

    // Capture edge calculations matching control pipeline handshake step
    if (top->out_valid) {
      if (out_pixel_count < output_pixels.size()) {
        output_pixels[out_pixel_count++] = top->pixel_out;
      }
    }

    top->clk = 0; // Clock Falling Edge Trigger Point
    top->eval();
    total_cycles++;

    // Safety watchdog threshold to protect against hardware lock state lockups
    if (total_cycles > (input_pixels.size() + 5000)) {
      std::cerr
          << "Simulation Error: Timeout. Sinks desynced from control line.\n";
      break;
    }
  }

  std::cout << "Processed frame successfully in " << total_cycles
            << " clock cycles.\n";

  // 6. Write Filtered Output Data Array Back to Disk Format
  std::ofstream outfile(argv[2], std::ios::binary);
  if (!outfile) {
    std::cerr << "Error: Could not open output file target path: " << argv[2]
              << "\n";
    return 1;
  }

  // Adjust out headers to reflect output payload footprint requirements
  info_header.bit_count = 24;
  info_header.image_size = output_row_padded * height;
  header.size =
      sizeof(BMPHeader) + sizeof(BMPInfoHeader) + info_header.image_size;

  outfile.write(reinterpret_cast<const char *>(&header), sizeof(header));
  outfile.write(reinterpret_cast<const char *>(&info_header),
                sizeof(info_header));

  std::vector<uint8_t> out_row_buffer(output_row_padded, 0);
  for (int y = height - 1; y >= 0; --y) {
    for (int x = 0; x < info_header.width; ++x) {
      uint8_t edge_val = output_pixels[y * info_header.width + x];
      // Re-populate all 3 color channels to output a standard grayscale BMP
      out_row_buffer[x * 3 + 0] = edge_val; // B
      out_row_buffer[x * 3 + 1] = edge_val; // G
      out_row_buffer[x * 3 + 2] = edge_val; // R
    }
    outfile.write(reinterpret_cast<const char *>(out_row_buffer.data()),
                  output_row_padded);
  }
  outfile.close();
  std::cout << "Output edge detection map successfully exported to: " << argv[2]
            << "\n";

  return 0;
}
