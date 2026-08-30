cmake_minimum_required(VERSION 3.20)
project(prosophor_core VERSION ${PACKAGE_VERSION_CLEAN} LANGUAGES CXX C)

# ============================================================================
# prosophor_core: common + platform + core logic (single static library)
# Used by both TUI and SDL builds
# ============================================================================
file(GLOB_RECURSE CORE_SOURCES_ALL
    ${CMAKE_CURRENT_LIST_DIR}/base/common/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/base/rtc_wrapper/resampler.cc
    ${CMAKE_CURRENT_LIST_DIR}/base/rtc_wrapper/vad_processor.cc
    ${CMAKE_CURRENT_LIST_DIR}/base/rtc_wrapper/rtc/*.c
    ${CMAKE_CURRENT_LIST_DIR}/base/rtc_wrapper/rtc/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/prosophor_core/*.cc
)

# Platform-specific sources
if(WIN32)
    set(PLATFORM_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/base/platform/platform.cc
        ${CMAKE_CURRENT_LIST_DIR}/base/platform/input_handler_win32.cc
        ${CMAKE_CURRENT_LIST_DIR}/base/platform/pipe_handler_win32.cc
    )
else()
    set(PLATFORM_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/base/platform/platform.cc
        ${CMAKE_CURRENT_LIST_DIR}/base/platform/input_handler_posix.cc
        ${CMAKE_CURRENT_LIST_DIR}/base/platform/pipe_handler_posix.cc
    )
endif()
list(APPEND CORE_SOURCES_ALL ${PLATFORM_SOURCES})

if(NOT PROSOPHOR_BUILD_LLAMA)
    list(FILTER CORE_SOURCES_ALL EXCLUDE REGEX "llamacpp_provider\\.cc$")
endif()

add_library(prosophor_core STATIC ${CORE_SOURCES_ALL})

target_include_directories(prosophor_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/prosophor_core>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/base>
)

target_compile_definitions(prosophor_core PUBLIC
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    PROSOPHOR_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(prosophor_core
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    PRIVATE
        CURL::libcurl
        OpenSSL::SSL
        OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(prosophor_core PUBLIC ws2_32 bcrypt iphlpapi)
endif()

if(PROSOPHOR_BUILD_LLAMA)
    target_include_directories(prosophor_core
        PUBLIC
            SYSTEM ${CMAKE_SOURCE_DIR}/thirdparty_src/llamacpp-src
            SYSTEM ${CMAKE_SOURCE_DIR}/thirdparty_src/llamacpp-src/common
            SYSTEM ${CMAKE_SOURCE_DIR}/thirdparty_src/llamacpp-src/ggml/include
    )
    target_compile_definitions(prosophor_core PUBLIC PROSOPHOR_HAS_LOCAL_MODEL)
    target_link_libraries(prosophor_core PUBLIC llama ggml llama-common)
endif()

# ============================================================================
# web_server: HTTP + WebSocket 多用户服务端(prosophor_web 可执行文件)。
# 含 IM 渠道层(原 im_gateway,唯一渠道 web):channel.h(ImChannel 抽象)+
# web_gateway(会话编排)+ web_channel + web_protocol(帧协议)。
# 依赖 prosophor_core;不链接 SDL/media_engine/components 等 UI 层
# (CLAUDE.md layering)。httplib 单头文件走 SYSTEM include 规避 -Werror。
# ============================================================================
file(GLOB_RECURSE WEB_SERVER_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/web_server/*.cc
)
list(FILTER WEB_SERVER_SOURCES EXCLUDE REGEX "main_web\\.cc$")

add_library(prosophor_web_server STATIC ${WEB_SERVER_SOURCES})

target_include_directories(prosophor_web_server
    SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty_src/cpp-httplib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/web_server>
)

target_link_libraries(prosophor_web_server
    PUBLIC prosophor_core
)

if(WIN32)
    target_link_libraries(prosophor_web_server PUBLIC ws2_32)
endif()

add_executable(prosophor_web
    ${CMAKE_CURRENT_LIST_DIR}/web_server/main_web.cc
    ${CMAKE_CURRENT_LIST_DIR}/resources/app.rc
)

# include 路径经 prosophor_web_server 的 PUBLIC BUILD_INTERFACE 传递
target_link_libraries(prosophor_web PRIVATE prosophor_web_server)

# llama-common 自带 vendor cpp-httplib(v0.45,编译为静态库),与本项目 v0.53
# header-only 符号冲突;取第一个定义(本项目 v0.53,API 向后兼容)。
target_link_options(prosophor_web PRIVATE "-Wl,--allow-multiple-definition")

# 前端产物(可选:PROSOPHOR_BUILD_WEB_FRONTEND=ON 时 npm 构建,默认 OFF 不阻塞无 Node 环境)
option(PROSOPHOR_BUILD_WEB_FRONTEND "Build web frontend via npm" OFF)
if(PROSOPHOR_BUILD_WEB_FRONTEND)
    find_program(NPM_EXECUTABLE npm)
    if(NPM_EXECUTABLE)
        add_custom_target(prosophor_web_frontend
            COMMAND ${NPM_EXECUTABLE} ci
            COMMAND ${NPM_EXECUTABLE} run build
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/web
            BYPRODUCTS ${CMAKE_SOURCE_DIR}/web/dist
            COMMENT "Building web frontend (npm ci && npm run build)"
        )
        add_dependencies(prosophor_web prosophor_web_frontend)
    else()
        message(WARNING "PROSOPHOR_BUILD_WEB_FRONTEND=ON but npm not found; frontend skipped")
    endif()
endif()

# ============================================================================
# TUI build: prosophor_core + ai_coding, no SDL/media/scene
# ============================================================================
if(NOT PROSOPHOR_SDL_UI)
    file(GLOB_RECURSE TUI_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/ai_coding/*.cc
    )

    add_executable(prosophor ${TUI_SOURCES} main.cc
        ${CMAKE_CURRENT_LIST_DIR}/resources/app.rc
    )

    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/resources/app.rc
        PROPERTIES SOURCE_FILE_DEPENDENCIES ${CMAKE_CURRENT_LIST_DIR}/resources/app.ico
    )

    target_include_directories(prosophor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/ai_coding
    )

    target_link_libraries(prosophor PRIVATE prosophor_core)
endif()

# ============================================================================
# SDL build: prosophor_core + media_engine + virtual_sprite
# ============================================================================
if(PROSOPHOR_SDL_UI)
    add_subdirectory(base/media_engine)

    target_compile_definitions(media_engine PRIVATE PROSOPHOR_VERSION="${PROJECT_VERSION}")

    add_compile_definitions(PROSOPHOR_SDL_UI)

    # components/ 已并入 virtual_sprite/（virtual_sprite/components/），
    # GLOB_RECURSE 自动覆盖，不再单独 glob。
    file(GLOB_RECURSE GUI_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/virtual_sprite/*.cc
    )
    add_executable(prosophor
        main.cc
        ${GUI_SOURCES}
        ${CMAKE_CURRENT_LIST_DIR}/resources/app.rc
    )

    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/resources/app.rc
        PROPERTIES SOURCE_FILE_DEPENDENCIES ${CMAKE_CURRENT_LIST_DIR}/resources/app.ico
    )

    # if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo|MinSizeRel")
    if(WIN32)
        set_target_properties(prosophor PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()

    target_include_directories(prosophor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/base
        ${CMAKE_CURRENT_SOURCE_DIR}/virtual_sprite
    )

    target_link_libraries(prosophor PRIVATE
        prosophor_core
        media_engine
    )
endif()

# ============================================================================
# Compiler flags
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(prosophor_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror -O2)
    target_compile_options(prosophor        PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror -O2)
    target_compile_options(prosophor_web_server PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror -O2)
    target_compile_options(prosophor_web    PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror -O2)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(prosophor_core PRIVATE /W4 /WX /Od /RTC1 /EHsc)
        target_compile_options(prosophor        PRIVATE /W4 /WX /Od /RTC1 /EHsc)
        target_compile_options(prosophor_web_server PRIVATE /W4 /WX /Od /RTC1 /EHsc)
        target_compile_options(prosophor_web    PRIVATE /W4 /WX /Od /RTC1 /EHsc)
    else()
        target_compile_options(prosophor_core PRIVATE /W4 /WX /O2 /EHsc)
        target_compile_options(prosophor        PRIVATE /W4 /WX /O2 /EHsc)
        target_compile_options(prosophor_web_server PRIVATE /W4 /WX /O2 /EHsc)
        target_compile_options(prosophor_web    PRIVATE /W4 /WX /O2 /EHsc)
    endif()
endif()

install(TARGETS prosophor DESTINATION bin)
install(TARGETS prosophor_web DESTINATION bin)
# 尾部斜杠:把 dist 内容直接装进 bin/web-dist(运行时静态根),而不是 web-dist/dist
install(DIRECTORY ${CMAKE_SOURCE_DIR}/web/dist/ DESTINATION bin/web-dist OPTIONAL)

