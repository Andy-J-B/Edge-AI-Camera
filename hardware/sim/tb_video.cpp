#include "Vsobel_filter.h"
#include "verilated.h"
#include <iostream>
#include <memory>
#include <opencv2/opencv.hpp>
#include <vector>

const int WIDTH = 640;
const int HEIGHT = 360;

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);

  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <input_video.mp4> <output_video.mp4>"
              << std::endl;
    return -1;
  }

  std::string inputPath = argv[1];
  std::string outputPath = argv[2];

  // 1. Open input video file
  cv::VideoCapture cap(inputPath);
  if (!cap.isOpened()) {
    std::cerr << "Error: Could not open input video file!" << std::endl;
    return -1;
  }

  double fps = cap.get(cv::CAP_PROP_FPS);
  int totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
  std::cout << "Processing video: " << WIDTH << "x" << HEIGHT << " @ " << fps
            << " FPS (" << totalFrames << " total frames)" << std::endl;

  // 2. Initialize video writer (using standard MJPEG codec inside an MP4
  // container)
  cv::VideoWriter writer(outputPath,
                         cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps,
                         cv::Size(WIDTH, HEIGHT), false);
  if (!writer.isOpened()) {
    std::cerr << "Error: Could not open output video file for writing!"
              << std::endl;
    return -1;
  }

  // 3. Instantiate Verilated Sobel Core
  auto top = std::make_unique<Vsobel_filter>();

  // Initial hardware reset pulse
  top->clk = 0;
  top->rst_n = 0;
  top->in_valid = 0;
  top->pixel_in = 0;
  top->eval();
  top->clk = 1;
  top->eval();
  top->clk = 0;
  top->rst_n = 1;
  top->eval();

  cv::Mat frame, grayscaleFrame;
  int frameCount = 0;

  // Allocate memory allocations for frame sequencing
  std::vector<uint8_t> outputBuffer(WIDTH * HEIGHT, 0);

  // 4. Video Frame Streaming Loop
  while (cap.read(frame)) {
    // Ensure incoming frame fits hardware canvas bounds perfectly
    cv::resize(frame, frame, cv::Size(WIDTH, HEIGHT));
    cv::cvtColor(frame, grayscaleFrame, cv::COLOR_BGR2GRAY);

    int inputIndex = 0;
    int outputIndex = 0;

    // Stream a single frame through the hardware core
    while (inputIndex < (WIDTH * HEIGHT) || outputIndex < (WIDTH * HEIGHT)) {
      top->clk = 0;

      if (inputIndex < (WIDTH * HEIGHT)) {
        top->in_valid = 1;
        top->pixel_in = grayscaleFrame.data[inputIndex];
      } else {
        top->in_valid = 0;
        top->pixel_in = 0;
      }

      top->eval();
      top->clk = 1;
      top->eval();

      if (inputIndex < (WIDTH * HEIGHT)) {
        inputIndex++;
      }

      if (top->out_valid) {
        if (outputIndex < (WIDTH * HEIGHT)) {
          outputBuffer[outputIndex] = top->pixel_out;
          outputIndex++;
        }
      }
    }

    // Create an OpenCV frame from our processed output buffer and write it
    cv::Mat outFrame(HEIGHT, WIDTH, CV_8UC1, outputBuffer.data());
    writer.write(outFrame);

    frameCount++;
    if (frameCount % 30 == 0 || frameCount == totalFrames) {
      std::cout << "Progress: " << frameCount << "/" << totalFrames
                << " frames processed." << std::endl;
    }
  }

  std::cout << "\nSuccess! Video acceleration complete." << std::endl;

  cap.release();
  writer.release();
  top->final();
  return 0;
}
