PROJECT_DIR ?= $(abspath ./)
CMAKE ?= cmake
MAKE ?= make
NUM_JOB ?= 8

# Set PATH for MSYS2/MinGW tools (always needed on this system)
export PATH := /e/devtool/msys64/mingw64/bin:$(PATH)

PACKAGE_NAME ?= Demo
PACKAGE_VERSION ?= v0.0.1
BUILD_TYPE ?= RelWithDebInfo


all:
	@echo hello world
.PHONY: all



BUILD_DIR ?= $(PROJECT_DIR)/build
INSTALL_DIR ?= $(BUILD_DIR)/install
CMAKE_ARGS ?= \
	-DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) \
	-DPACKAGE_BUILD_DIR=$(BUILD_DIR) \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	$(CMAKE_EXTRA_ARGS)

build:
	mkdir -p ${BUILD_DIR} && \
	cd ${BUILD_DIR} && \
	${CMAKE} ${CMAKE_ARGS} .. && \
	${MAKE} -j${NUM_JOB}; \
	${MAKE} install
.PHONY: build

run:
	$(INSTALL_DIR)/bin/aicode;
.PHONY: run

clean:
	find ${BUILD_DIR} -mindepth 1 -not -path "*/_deps*" -delete
.PHONY: clean



BUILD_DIR_WIN ?= $(PROJECT_DIR)/build_win
INSTALL_DIR_WIN ?= $(BUILD_DIR_WIN)/install
CMAKE_ARGS_WIN ?= \
	-DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR_WIN) \
	-DPACKAGE_BUILD_DIR=$(BUILD_DIR_WIN) \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	$(CMAKE_EXTRA_ARGS)


build_win:
	mkdir -p ${BUILD_DIR_WIN} && \
	cd ${BUILD_DIR_WIN} && \
	${CMAKE} ${CMAKE_ARGS_WIN} .. && \
	ninja -j${NUM_JOB}; \
	ninja install
.PHONY: build_win


run_win:
	/e/devtool/msys64/msys2_shell.cmd -defterm -here -no-start -mingw64 -c "cd ${INSTALL_DIR_WIN}/bin && ./aicode.exe"; \
.PHONY: run_win


clean_win:
	find ${BUILD_DIR_WIN} -mindepth 1 -not -path "*/_deps*" -delete
.PHONY: clean_win



