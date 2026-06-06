cmake_minimum_required(VERSION 3.20)
project(prosophor_core VERSION ${PACKAGE_VERSION_CLEAN} LANGUAGES CXX C)

# ============================================================================
# prosophor_core: common + platform + core logic (single static library)
# Used by both TUI and SDL builds
# ============================================================================
file(GLOB_RECURSE CORE_SOURCES_ALL
    ${CMAKE_CURRENT_LIST_DIR}/common/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/prosophor_core/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/voice/rtc/*.c
    ${CMAKE_CURRENT_LIST_DIR}/voice/rtc/*.cc
)

# Platform-specific sources
if(WIN32)
    set(PLATFORM_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/platform/platform.cc
        ${CMAKE_CURRENT_LIST_DIR}/platform/input_handler_win32.cc
        ${CMAKE_CURRENT_LIST_DIR}/platform/pipe_handler_win32.cc
    )
else()
    set(PLATFORM_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/platform/platform.cc
        ${CMAKE_CURRENT_LIST_DIR}/platform/input_handler_posix.cc
        ${CMAKE_CURRENT_LIST_DIR}/platform/pipe_handler_posix.cc
    )
endif()
list(APPEND CORE_SOURCES_ALL ${PLATFORM_SOURCES})

add_library(prosophor_core STATIC ${CORE_SOURCES_ALL})

target_include_directories(prosophor_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/prosophor_core>
)

target_compile_definitions(prosophor_core PUBLIC
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    PROSOPHOR_VERSION="${PROJECT_VERSION}"
    PROSOPHOR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
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
    target_link_libraries(prosophor_core PUBLIC ws2_32 bcrypt)
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
    add_subdirectory(media_engine)

    add_compile_definitions(PROSOPHOR_SDL_UI)

    file(GLOB_RECURSE GUI_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/virtual_sprite/*.cc
    )

    file(GLOB_RECURSE COMPONENTS_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/components/*.cc
    )
    file(GLOB_RECURSE VOICE_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/voice/*.cc
    )
    add_executable(prosophor
        main.cc
        ${GUI_SOURCES}
        ${COMPONENTS_SOURCES}
        ${VOICE_SOURCES}
        ${CMAKE_CURRENT_LIST_DIR}/resources/app.rc
    )

    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/resources/app.rc
        PROPERTIES SOURCE_FILE_DEPENDENCIES ${CMAKE_CURRENT_LIST_DIR}/resources/app.ico
    )

    if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo|MinSizeRel")
        set_target_properties(prosophor PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()

    target_include_directories(prosophor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/virtual_sprite
        ${CMAKE_CURRENT_SOURCE_DIR}/components
        ${CMAKE_CURRENT_SOURCE_DIR}/voice
    )

    target_compile_definitions(prosophor PRIVATE
        PROSOPHOR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
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
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(prosophor_core PRIVATE /W4 /WX /Od /RTC1 /EHsc)
        target_compile_options(prosophor        PRIVATE /W4 /WX /Od /RTC1 /EHsc)
    else()
        target_compile_options(prosophor_core PRIVATE /W4 /WX /O2 /EHsc)
        target_compile_options(prosophor        PRIVATE /W4 /WX /O2 /EHsc)
    endif()
endif()

install(TARGETS prosophor DESTINATION bin)

