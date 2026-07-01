//
#include "Vsobel_filter.h"
#include "verilated.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

const int WIDTH = 640;
const int HEIGHT = 360;

int main(int argc, char **argv) {
  // 1. Initialize Verilator arguments parsing context
  Verilated::commandArgs(argc, argv);

  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <input_grayscale.bmp> <output_edge.bmp>" << std::endl;
    return -1;
  }

  std::string inputPath = argv[1];
  std::string outputPath = argv[2];

  // 2. Read input BMP file
  std::ifstream inFile(inputPath, std::ios::binary);
  if (!inFile) {
    std::cerr << "Error: Could not open input file " << inputPath << std::endl;
    return -1;
  }

  uint8_t bmpHeader[54];
  inFile.read(reinterpret_cast<char *>(bmpHeader), 54);
  if (!inFile) {
    std::cerr << "Error: Invalid BMP header read." << std::endl;
    return -1;
  }

  // Allocate memory for frame buffers
  std::vector<uint8_t> inputBuffer(WIDTH * HEIGHT);
  std::vector<uint8_t> outputBuffer(WIDTH * HEIGHT, 0);

  // Read raw pixel payload
  inFile.read(reinterpret_cast<char *>(inputBuffer.data()), WIDTH * HEIGHT);
  inFile.close();

  // 3. Instantiate and initialize the Verilated Sobel Core module
  auto top = std::make_unique<Vsobel_filter>();

  top->clk = 0;
  top->rst_n = 0;
  top->in_valid = 0;
  top->pixel_in = 0;

  // Evaluate initial reset states
  top->eval();
  top->clk = 1;
  top->eval();
  top->clk = 0;
  top->rst_n = 1;
  top->eval();

  std::cout << "Streaming " << (WIDTH * HEIGHT)
            << " pixels through Sobel Core..." << std::endl;

  int inputIndex = 0;
  int outputIndex = 0;
  uint64_t cycleCount = 0;

  // 4. Main Simulation Loop
  // Continue running until all input pixels are pushed and processed outputs
  // are captured
  while (inputIndex < (WIDTH * HEIGHT) || outputIndex < (WIDTH * HEIGHT)) {
    top->clk = 0;

    // Feed data if we still have incoming input pixels remaining
    if (inputIndex < (WIDTH * HEIGHT)) {
      top->in_valid = 1;
      top->pixel_in = inputBuffer[inputIndex];
    } else {
      top->in_valid = 0; // Clear valid flag once input frame completes
      top->pixel_in = 0;
    }

    top->eval();

    // Rising Edge Evaluation
    top->clk = 1;
    top->eval();

    // Increment data index on accepted input handshake cycle
    if (inputIndex < (WIDTH * HEIGHT)) {
      inputIndex++;
    }

    // Capture processed core stream arrays on active output valid cycle
    // handshakes
    if (top->out_valid) {
      if (outputIndex < (WIDTH * HEIGHT)) {
        outputBuffer[outputIndex] = top->pixel_out;
        if (outputIndex < 10) {
          std::cout << "Debug Pixel " << outputIndex << ": "
                    << (int)top->pixel_out << std::endl;
        }
        outputIndex++;
      }
    }

    cycleCount++;

    // Watchdog failsafe step to prevent infinite looping conditions on hardware
    // stall bugs
    if (cycleCount > (WIDTH * HEIGHT * 2)) {
      std::cerr << "Simulation Watchdog Timeout: Check your hardware handshake "
                   "flow logic!"
                << std::endl;
      return -1;
    }
  }

  std::cout << "Processed frame successfully in " << cycleCount
            << " clock cycles." << std::endl;

  // 5. Build Airtight BMP Headers for Export
  uint32_t totalPixelBytes = WIDTH * HEIGHT;

  const int paletteSize = 256 * 4;
  const int fullHeaderSize = 54 + paletteSize;
  uint32_t totalFileSize = fullHeaderSize + totalPixelBytes;

  std::vector<uint8_t> completeHeader(fullHeaderSize, 0);

  // Populate standard Bitmap File Header (14 bytes)
  completeHeader[0] = 'B';
  completeHeader[1] = 'M';
  completeHeader[2] = static_cast<uint8_t>(totalFileSize);
  completeHeader[3] = static_cast<uint8_t>(totalFileSize >> 8);
  completeHeader[4] = static_cast<uint8_t>(totalFileSize >> 16);
  completeHeader[5] = static_cast<uint8_t>(totalFileSize >> 24);
  completeHeader[10] =
      static_cast<uint8_t>(fullHeaderSize); // Pixel data offset

  // Populate DIB Info Header (40 bytes)
  completeHeader[14] = 40; // Info header size
  completeHeader[18] = static_cast<uint8_t>(WIDTH);
  completeHeader[19] = static_cast<uint8_t>(WIDTH >> 8);
  completeHeader[20] = static_cast<uint8_t>(WIDTH >> 16);
  completeHeader[21] = static_cast<uint8_t>(WIDTH >> 24);

  completeHeader[22] = static_cast<uint8_t>(HEIGHT);
  completeHeader[23] = static_cast<uint8_t>(HEIGHT >> 8);
  completeHeader[24] = static_cast<uint8_t>(HEIGHT >> 16);
  completeHeader[25] = static_cast<uint8_t>(HEIGHT >> 24);

  completeHeader[26] = 1; // Planes
  completeHeader[28] = 8; // Bits per pixel (8-bit Grayscale)
  completeHeader[34] = static_cast<uint8_t>(totalPixelBytes); // Image size
  completeHeader[46] = 0; // Colors in palette (0 defaults to 2^8 = 256)

  // Map index i to uniform BGRA intensities [i, i, i, 0] so 255 becomes pure
  // white, not pink!
  for (int i = 0; i < 256; ++i) {
    int baseOffset = 54 + (i * 4);
    completeHeader[baseOffset] = i;     // Blue
    completeHeader[baseOffset + 1] = i; // Green
    completeHeader[baseOffset + 2] = i; // Red
    completeHeader[baseOffset + 3] = 0; // Reserved
  }

  // 6. Secure Export Implementation with Explicit Storage Flushing
  std::ofstream outFile(outputPath,
                        std::ios::out | std::ios::binary | std::ios::trunc);
  if (!outFile) {
    std::cerr << "Error: Could not open output file path target " << outputPath
              << std::endl;
    return -1;
  }

  outFile.write(reinterpret_cast<const char *>(completeHeader.data()),
                fullHeaderSize);
  outFile.write(reinterpret_cast<const char *>(outputBuffer.data()),
                totalPixelBytes);

  outFile.flush();
  outFile.close();

  std::cout << "Output edge detection map successfully exported with Grayscale "
               "palette."
            << std::endl;

  // Final clean tear down
  top->final();
  return 0;
}
