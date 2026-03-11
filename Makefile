BUILD_DIR ?= build
CONFIG ?= Release

.PHONY: all configure build run clean

all: build

configure:
	@cmake -S . -B $(BUILD_DIR)

build: configure
	@cmake --build $(BUILD_DIR) --config $(CONFIG)

run: build
	@powershell -NoProfile -Command "& '.\\$(BUILD_DIR)\\opengl_sky.exe'"

clean:
	@powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }"
