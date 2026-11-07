SHELL := /bin/bash
.DEFAULT_GOAL := app
BUILD_JOBS ?= 4
USE_CCACHE ?= 1
export BUILD_JOBS USE_CCACHE
export APP_DEFINITIONS := GL_GLEXT_PROTOTYPES=1 JSON_NOEXCEPTION=1
export APP_INCLUDE_PATHS := src .deps/ps5-opengl/current/include
export APP_STATIC_ARCHIVES := .deps/ps5-opengl/libps5opengl-group.a
export APP_IMPORT_STUBS := .deps/ps5-opengl/current/lib/libSceAgc.so .deps/ps5-opengl/current/lib/libSceAgcDriver.so \
	.deps/stubs/libSceAudioIn.so
.PHONY: app test preview fonts lint deploy format payload tour
app:
	bash tools/rebuild-libc.sh
	bash tools/prepare-opengl.sh
	bash tools/stubs.sh
	bash tools/build.sh Folder
test:
	bash tools/test.sh
preview:
	bash tools/preview.sh
tour:
	bash tools/tour-video.sh
fonts:
	bash tools/fonts.sh
lint:
	python3 tools/check.py
deploy:
	python3 tools/deploy.py
format:
	clang-format-18 -i src/main.cpp src/lamplight/*.cpp src/lamplight/*.hpp payload/*.cpp payload/*.hpp host/preview.cpp host/tour.cpp host/offscreen.hpp host/fixture_home.hpp tests/tests.cpp
payload:
	bash tools/payload.sh
