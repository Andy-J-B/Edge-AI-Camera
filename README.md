# Edge-AI-Camera

**Hardware-Accelerated Edge AI Smart Camera System**

A self-contained, heterogeneous SoC architecture that runs the entire camera pipeline locally — raw pixel ingest, hardware pre-processing, zero-copy transport, and neural network inference — with **zero computation offloaded to an external server**.

## Architecture

The workload is split across three domains based on their processing strengths:

```
[ Camera Sensor ]
       │ (Raw Pixel Stream)
       ▼
[ 1. Verilog Core ] ────> (Pre-processes, crops/filters pixels, flashes RTOS)
       │
       │ (Blasts frame into RAM via DMA)
       ▼
[ 2. Linux Driver ] ────> (Catches hardware interrupt, passes memory pointer to App)
       │
       ▼
[ 3. C++ AI App ]   ────> (Reads frame directly from RAM, runs local AI inference,
                           draws bounding boxes, streams video to network)
       │
  (Sends RPMsg Sync)
       ▼
[ 4. RTOS Core ]    ────> (Catches sync message/interrupt, drives physical
                           motors dynamically to keep camera pointed at target)
```

### 1. Hardware Fabric (Verilog)

- **Input:** Raw pixel streams from a camera sensor (or simulation).
- **Computation:** Real-time pixel-pipeline acceleration — matrix operations (Sobel edge detection), color-space conversion, and downscaling to the model's input resolution.
- **Output:** A custom **DMA engine** writes pre-processed frames directly into shared DDR memory via AXI4, bypassing the CPU.

### 2. Local Host CPU (Embedded Linux & AI)

- **Kernel space:** A custom driver manages frame allocation queues via the Linux `videobuf2` framework and exposes them to user space.
- **User space:** A lightweight C++ runtime (ONNX Runtime / TensorFlow Lite) loads a quantized neural network (Tiny-YOLO / MobileNet), computes object classification, and draws bounding boxes.
- **Output:** Streams the AI-annotated feed to a local display or RTSP stream, and logs detection telemetry.

### 3. Real-Time Layer (RTOS, planned)

- Runs independently on a dedicated micro-controller core for **deterministic, hard real-time safety and physical control**.
- Handles interrupts from the Verilog fabric and drives pan/tilt motors to keep targets centered.
- Communicates with the Linux host via RPMsg.

## Repository Structure

```text
├── hardware/
│   ├── rtl/                    # Custom Verilog sources
│   │   ├── sobel_filter.v      # 3x3 Sobel edge-detection core
│   │   └── line_buffer.v       # Streaming line-buffer for the convolution window
│   └── sim/                    # Verilator testbenches
│       ├── tb_sobel.cpp        # Single-image (BMP) pipeline test
│       └── tb_video.cpp        # Continuous video (OpenCV) pipeline test
├── kernel/                     # Custom Linux kernel driver (V4L2/videobuf2, WIP)
│   ├── edge_camera.c
│   ├── edge_camera.h
│   └── Makefile
├── app/                        # Local C++ AI inference application (planned)
├── scripts/                    # Environmental init & FFmpeg loopbacks (planned)
├── test/assets/                # Sample images & footage
└── .github/workflows/          # RTL & video CI pipelines
```

## Getting Started

### Prerequisites

```bash
brew install verilator cmake opencv   # macOS
# or on Linux:
sudo apt-get install verilator cmake libopencv-dev
```

### Build

```bash
make configure   # Generate the CMake build matrix
make build       # Compile Verilator simulation targets
```

### Test the Verilator Hardware Pipeline

Process a single image through the Sobel core:

```bash
make run
# Converts test/assets/test_image.png → 640x360 grayscale BMP,
# streams it through the Verilog core, writes test/assets/output_edge.bmp
```

Process video footage (samples every 30th frame):

```bash
make run-video
# Writes test/assets/output_edge.mp4
```

Or run the binaries directly:

```bash
./build/Vsobel_filter test/assets/input_grayscale.bmp test/assets/output_edge.bmp
./build/Vvideo_filter test/assets/test_footage.mp4 test/assets/output_edge.mp4
```

## Roadmap

### Phase 1 — Hardware Design (Verilog) ✅ Core complete

- [x] Sobel edge-detection pixel core with 3x3 convolution window
- [x] Streaming line buffers with exact pipeline-latency alignment
- [x] Verilator cycle-accurate testbenches (image + video)
- [x] CI workflows for both pipelines
- [ ] AXI4-Lite target interface for host CPU register access (e.g. thresholds)
- [ ] AXI4 Initiator DMA engine to write filtered frames to physical memory

### Phase 2 — Linux Kernel Driver (C, in progress)

- [ ] Platform driver registration and `/dev/videoX` node creation
- [ ] V4L2 device + `videobuf2` queue setup (`vb2_ops`)
- [ ] DMA-coherent buffer allocation with `mmap()` for zero-copy userspace access
- [ ] DMA engine integration: pass buffer addresses to hardware, signal completion
- [ ] Control IOCTLs (`QUERYCAP`, `ENUM_FMT`, `S/G_FMT`, `REQBUFS`, `QBUF`, `STREAMON/OFF`)
- [ ] Device Tree binding for register regions and shared memory
- [ ] Kernel build integration (`Makefile`)

### Phase 3 — Local Inference & Application (C++)

- [ ] User-space app pulling frames from `/dev/videoX` via `mmap`
- [ ] ONNX Runtime / TensorFlow Lite integration with a quantized Tiny-YOLO model
- [ ] Bounding-box extraction and overlay onto the frame
- [ ] RTSP / local display output

### Phase 4 — Real-Time Layer (RTOS)

- [ ] FreeRTOS/Zephyr on a secondary core
- [ ] ISR handling for hardware detection flags
- [ ] Motor control (PID / step generation) for gimbal tracking
- [ ] RPMsg inter-processor communication with the Linux host

## Simulation Tech Stack

| Component | Tool |
|---|---|
| Hardware simulation | Verilator (cycle-accurate C++ classes) |
| Camera sensor simulation | FFmpeg + v4l2loopback (`/dev/video10`) |
| SoC emulation | QEMU (planned) |
| Inference runtime | ONNX Runtime C++ / TensorFlow Lite (planned) |

## License

TBD.
