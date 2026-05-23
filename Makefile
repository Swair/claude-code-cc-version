# ==============================================================================
# Prosophor Makefile
# ==============================================================================
# 编译目标说明：
#   1. build        - Linux/macOS 构建 (使用 system 默认编译器)
#   2. build_win    - Windows 构建 (MSYS2/MinGW, 无 SDL UI)
#   3. build_win_sdl- Windows 构建 (MSYS2/MinGW, 启用 SDL UI)
# ==============================================================================

PROJECT_DIR ?= $(abspath ./)
CMAKE ?= cmake
MAKE ?= make
NUM_JOB ?= 8

# Set PATH for MSYS2/MinGW tools (always needed on this system)
export PATH := /e/devtool/msys64/mingw64/bin:$(PATH)

PACKAGE_NAME ?= Prosophor
PACKAGE_VERSION ?= 0.6.3
BUILD_TYPE ?= RelWithDebInfo
# BUILD_TYPE ?= Debug


all:
	@echo hello world
.PHONY: all


# ==============================================================================
# Linux/macOS 构建配置
# ==============================================================================

BUILD_DIR ?= $(PROJECT_DIR)/build
INSTALL_DIR ?= $(BUILD_DIR)/install
CMAKE_ARGS ?= \
	-DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR) \
	-DPACKAGE_BUILD_DIR=$(BUILD_DIR) \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DPACKAGE_VERSION=$(PACKAGE_VERSION) \
	-DPROSOPHOR_BUILD_LLAMA=ON \
	$(CMAKE_EXTRA_ARGS)

build:
	mkdir -p ${BUILD_DIR} && \
	cd ${BUILD_DIR} && \
	${CMAKE} ${CMAKE_ARGS} .. && \
	${MAKE} -j${NUM_JOB}; \
	${MAKE} install
.PHONY: build

run:
	$(INSTALL_DIR)/bin/prosophor
.PHONY: run


tests:
	@echo "Running all tests in $(BUILD_DIR)/unitests..."
	@for test in $(INSTALL_DIR)/bin/unitests/*_test; do \
		if [ -x "$$test" ] && [ ! -d "$$test" ]; then \
			$$test --gtest_list_tests >/dev/null 2>&1 || continue; \
			echo "=== $$(basename $$test) ==="; \
			$$test || exit 1; \
		fi \
	done
.PHONY: tests

clean:
	rm -rf ${BUILD_DIR}
.PHONY: clean

# ==============================================================================
# Windows 构建配置 (MSYS2/MinGW)
# ==============================================================================

BUILD_DIR_WIN ?= $(PROJECT_DIR)/build_win
INSTALL_DIR_WIN ?= $(BUILD_DIR_WIN)/install
CMAKE_ARGS_WIN ?= \
	-DCMAKE_INSTALL_PREFIX=$(INSTALL_DIR_WIN) \
	-DPACKAGE_BUILD_DIR=$(BUILD_DIR_WIN) \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DPACKAGE_VERSION=$(PACKAGE_VERSION) \
	$(CMAKE_EXTRA_ARGS)

# Windows 构建 - 无 SDL UI (纯终端模式)
build_win:
	mkdir -p ${BUILD_DIR_WIN} && \
	cd ${BUILD_DIR_WIN} && \
	${CMAKE} ${CMAKE_ARGS_WIN} -DPROSOPHOR_SDL_UI=OFF .. && \
	ninja -j${NUM_JOB}; \
	ninja install
.PHONY: build_win

# Windows 构建 - 启用 SDL UI (图形界面模式)
build_win_sdl:
	mkdir -p ${BUILD_DIR_WIN} && \
	cd ${BUILD_DIR_WIN} && \
	${CMAKE} ${CMAKE_ARGS_WIN} -DPROSOPHOR_SDL_UI=ON .. && \
	ninja -j${NUM_JOB}; \
	ninja install
.PHONY: build_win_sdl

run_win:
	cd $(INSTALL_DIR_WIN)/bin && SSL_CERT_FILE=ca-bundle.crt ./prosophor.exe
.PHONY: run_win

clean_win:
	rm -rf ${BUILD_DIR_WIN}
.PHONY: clean_win

# NSIS 安装包路径
NSIS ?= /e/devtool/msys64/mingw64/bin/makensis.exe
PACKAGE_DIR ?= $(PROJECT_DIR)/tools/packaging

package: build_win_sdl
	cp /e/devtool/msys64/mingw64/etc/ssl/certs/ca-bundle.crt $(INSTALL_DIR_WIN)/bin/
	cd $(PACKAGE_DIR) && "$(NSIS)" -DPRODUCT_VERSION=$(PACKAGE_VERSION) installer.nsi
	mv $(PACKAGE_DIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-win64-setup.exe $(BUILD_DIR_WIN)/
	@echo "=== Installer created: $(BUILD_DIR_WIN)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-win64-setup.exe ==="
.PHONY: package

# 构建+NSIS 安装包+发布到 GitHub Releases（需要先 git tag）
deploy: package
	@echo "Creating GitHub release v$(PACKAGE_VERSION)..."
	gh release create v$(PACKAGE_VERSION) \
	  $(BUILD_DIR_WIN)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-win64-setup.exe \
	  --title "v$(PACKAGE_VERSION)" \
	  --generate-notes
.PHONY: deploy

GITEE_API ?= https://gitee.com/api/v5
GITEE_OWNER ?= swair
GITEE_REPO ?= prosophor
GITEE_TOKEN ?= $(GITEE_ACCESS_TOKEN)
GITEE_RELEASE_BODY ?= Release v$(PACKAGE_VERSION)

# 构建+NSIS 安装包+发布到 Gitee 发行版（需要先 git tag，需设置 GITEE_TOKEN）
deploy_gitee: package
	@if [ -z "$(GITEE_TOKEN)" ]; then \
	  echo "GITEE_TOKEN is required. Example: make deploy_gitee GITEE_TOKEN=xxxxx"; \
	  exit 1; \
	fi
	@echo "Creating Gitee release v$(PACKAGE_VERSION)..."
	@release_id=$$(curl -sS -X POST "$(GITEE_API)/repos/$(GITEE_OWNER)/$(GITEE_REPO)/releases" \
	  -F "access_token=$(GITEE_TOKEN)" \
	  -F "tag_name=v$(PACKAGE_VERSION)" \
	  -F "name=v$(PACKAGE_VERSION)" \
	  -F "body=$(GITEE_RELEASE_BODY)" \
	  | python -c "import json,sys; data=json.load(sys.stdin); print(data.get('id',''))"); \
	if [ -z "$$release_id" ]; then \
	  echo "Failed to create Gitee release"; \
	  exit 1; \
	fi; \
	echo "Uploading installer to Gitee release $$release_id..."; \
	curl -sS -X POST "$(GITEE_API)/repos/$(GITEE_OWNER)/$(GITEE_REPO)/releases/$$release_id/attach_files" \
	  -F "access_token=$(GITEE_TOKEN)" \
	  -F "file=@$(BUILD_DIR_WIN)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-win64-setup.exe"; \
	echo "Gitee release created: v$(PACKAGE_VERSION)"
.PHONY: deploy_gitee

# 构建+NSIS 安装包+同时发布到 GitHub 和 Gitee
deploy_all: deploy deploy_gitee
.PHONY: deploy_all

# 运行所有单元测试 (执行 bin/tests 目录下所有测试程序)
.PHONY: run_win_tests
run_win_tests:
	@echo "Running all tests in $(INSTALL_DIR_WIN)/bin/tests..."
	@for test in $(INSTALL_DIR_WIN)/bin/tests/*.exe; do echo "Running $$(basename $$test)..."; $$test || exit 1; done



# ==============================================================================
# llamacpp server
# ==============================================================================

MODEL ?= $(PROJECT_DIR)/../llama_cpp_model/google_gemma-4-E4B-it-Q4_K_M.gguf
run_llamacpp_server:
	$(INSTALL_DIR)/bin/llama-server -m $(MODEL) --host 0.0.0.0 --port 8080
.PHONY: run_llamacpp_server

run_llamacpp_server_win:
	PATH="/e/devtool/msys64/mingw64/bin:$$PATH" $(INSTALL_DIR_WIN)/bin/llama-server.exe -m $(MODEL) --host 0.0.0.0 --port 8080
.PHONY: run_llamacpp_server_win

stop_llamacpp_server:
	-killall llama-server 2>/dev/null || pkill -f llama-server 2>/dev/null || true
.PHONY: stop_llamacpp_server

stop_llamacpp_server_win:
	-taskkill -IM llama-server.exe -F 2>/dev/null || true
.PHONY: stop_llamacpp_server_win

