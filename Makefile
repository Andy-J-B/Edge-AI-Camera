# Automation shortcuts
.PHONY: all configure build run run-video clean

all: configure build run

# 1. Generate build matrix from hardware subdirectory and symlink Neovim LSP
configure:
	cmake -B build -S hardware
	ln -sf build/compile_commands.json .

# 2. Compile the simulation target binary application
build:
	cmake --build build

# 3. Convert assets and stream the pixel data through the Sobel core
run:
	@echo "=== Step 1: Pre-processing PNG Asset to Explicit 640x360 Grayscale BMP ==="
	@python3 -c "from PIL import Image; img = Image.open('test/assets/test_image.png').convert('L').resize((640, 360)); img.save('test/assets/input_grayscale.bmp')" 2>/dev/null || \
	 python3 -c "import cv2; img = cv2.imread('test/assets/test_image.png', cv2.IMREAD_GRAYSCALE); img = cv2.resize(img, (640, 360)); cv2.imwrite('test/assets/input_grayscale.bmp', img)" || \
	 (echo "Error: Python standard environment requires PIL (Pillow) or OpenCV to handle PNG pre-processing!"; exit 1)
	
	@echo "=== Step 2: Executing Verilator Simulation ==="
	./build/Vsobel_filter test/assets/input_grayscale.bmp test/assets/output_edge.bmp

# 4. Process video footage through the Sobel core (samples every 30th frame)
run-video: INPUT_MP4 := test/assets/test_footage.mp4
run-video: FRAME_DIR := /tmp/sobel_frames
run-video: OUT_DIR := /tmp/sobel_output
run-video:
	@echo "=== Step 1: Extracting every 30th frame from $(INPUT_MP4) ==="
	rm -rf $(FRAME_DIR) $(OUT_DIR)
	mkdir -p $(FRAME_DIR)/input $(FRAME_DIR)/output
	ffmpeg -i $(INPUT_MP4) -vf "select=not(mod(n\,30)),scale=640:360,setsar=1" \
		-fps_mode vfr $(FRAME_DIR)/input/frame_%04d.bmp -loglevel error
	@echo "=== Step 2: Running Sobel simulation on each frame ==="
	@for f in $(FRAME_DIR)/input/*.bmp; do \
		base=$$(basename "$$f"); \
		echo "  Processing $$base..."; \
		./build/Vsobel_filter "$$f" "$(FRAME_DIR)/output/$$base"; \
	done
	@echo "=== Step 3: Reconstructing output video ==="
	ffmpeg -framerate 1 -i $(FRAME_DIR)/output/frame_%04d.bmp \
		-c:v libx264 -pix_fmt yuv420p test/assets/output_edge.mp4 -loglevel error
	@echo "=== Done: Output at test/assets/output_edge.mp4 ==="

# 5. Wipe out build infrastructure and local output files
clean:
	rm -rf build compile_commands.json test/assets/input_grayscale.bmp test/assets/output_edge.bmp
