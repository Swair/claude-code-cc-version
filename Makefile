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
# BUILD_TYPE ?= RelWithDebInfo
BUILD_TYPE ?= Debug


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
	-DGGML_VULKAN=ON \
	-DPROSOPHOR_BUILD_LLAMA=ON \
	-DPROSOPHOR_BUILD_LLAMA_CUDA=ON \
	-DPROSOPHOR_BUILD_ASR=ON \
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

tests:
	@echo "Running all tests in $(BUILD_DIR_WIN)/unitests..."
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
GITEE_TARGET_COMMITISH ?= main
GITEE_MAX_ASSET_BYTES ?= 104857600
GITEE_RELEASE_BODY ?= Release v$(PACKAGE_VERSION)

# 构建+NSIS 安装包+发布到 Gitee 发行版（需要先 git tag，需设置 GITEE_TOKEN）
deploy_gitee: package upload_gitee
.PHONY: deploy_gitee

# 仅发布已有安装包到 Gitee，避免 deploy_gitee 失败后重新打包
upload_gitee:
	@if [ -z "$(GITEE_TOKEN)" ]; then \
	  echo "GITEE_TOKEN is required. Example: make upload_gitee GITEE_TOKEN=xxxxx"; \
	  exit 1; \
	fi
	@if [ ! -f "$(BUILD_DIR_WIN)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-win64-setup.exe" ]; then \
	  echo "Installer not found: $(BUILD_DIR_WIN)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-win64-setup.exe"; \
	  exit 1; \
	fi
	@echo "Creating Gitee release v$(PACKAGE_VERSION)..."
	@response=$$(curl -sS -X POST "$(GITEE_API)/repos/$(GITEE_OWNER)/$(GITEE_REPO)/releases" \
	  -F "access_token=$(GITEE_TOKEN)" \
	  -F "tag_name=v$(PACKAGE_VERSION)" \
	  -F "target_commitish=$(GITEE_TARGET_COMMITISH)" \
	  -F "name=v$(PACKAGE_VERSION)" \
	  -F "body=$(GITEE_RELEASE_BODY)"); \
	release_id=$$(printf '%s' "$$response" | PYTHONIOENCODING=utf-8 python -c "import json,sys; data=json.load(sys.stdin); print(data.get('id',''))"); \
	if [ -z "$$release_id" ]; then \
	  printf '%s' "$$response" | PYTHONIOENCODING=utf-8 python -c "import json,sys; data=json.load(sys.stdin); print('Failed to create Gitee release: ' + str(data.get('message') or data.get('error') or data))"; \
	  exit 1; \
	fi; \
	installer="$(BUILD_DIR_WIN)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)-win64-setup.exe"; \
	installer_size=$$(wc -c < "$$installer"); \
	if [ "$$installer_size" -gt "$(GITEE_MAX_ASSET_BYTES)" ]; then \
	  echo "Gitee release created: v$(PACKAGE_VERSION)"; \
	  echo "Skip Gitee asset upload: $$installer_size bytes exceeds 100 MB limit"; \
	  exit 0; \
	fi; \
	echo "Uploading installer to Gitee release $$release_id..."; \
	upload_response=$$(curl -sS -X POST "$(GITEE_API)/repos/$(GITEE_OWNER)/$(GITEE_REPO)/releases/$$release_id/attach_files" \
	  -F "access_token=$(GITEE_TOKEN)" \
	  -F "file=@$$installer"); \
	upload_message=$$(printf '%s' "$$upload_response" | PYTHONIOENCODING=utf-8 python -c "import json,sys; data=json.load(sys.stdin); print(data.get('message',''))"); \
	if [ -n "$$upload_message" ]; then \
	  printf '%s' "$$upload_response" | PYTHONIOENCODING=utf-8 python -c "import json,sys; data=json.load(sys.stdin); print('Failed to upload Gitee asset: ' + str(data.get('message') or data))"; \
	  exit 1; \
	fi; \
	echo "Gitee release and asset created: v$(PACKAGE_VERSION)"
.PHONY: upload_gitee

# 构建+NSIS 安装包+同时发布到 GitHub 和 Gitee
deploy_all: deploy deploy_gitee
.PHONY: deploy_all

# 运行所有单元测试 (执行 bin/tests 目录下所有测试程序)
.PHONY: run_win_tests
run_win_tests:
	@echo "Running all tests in $(INSTALL_DIR_WIN)/bin/tests..."
	@for test in $(INSTALL_DIR_WIN)/bin/tests/*.exe; do echo "Running $$(basename $$test)..."; $$test || exit 1; done



