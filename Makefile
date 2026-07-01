# Automation shortcuts
.PHONY: all configure build run clean

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

# 4. Wipe out build infrastructure and local output files
clean:
	rm -rf build compile_commands.json test/assets/input_grayscale.bmp test/assets/output_edge.bmp
