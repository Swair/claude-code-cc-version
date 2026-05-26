cmake_minimum_required(VERSION 3.20)
project(prosophor_core VERSION ${PACKAGE_VERSION_CLEAN} LANGUAGES CXX)


include(FetchContent)

############################################# spdlog #####################################
if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty_src/spdlog-src")
    message(STATUS "Found spdlog in thirdparty_src, using existing download")
    FetchContent_Declare(spdlog SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty_src/spdlog-src")
else()
    message(STATUS "spdlog not found, fetching from GitHub...")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.0
        GIT_SHALLOW TRUE
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/thirdparty_src/spdlog-src
    )
endif()
FetchContent_MakeAvailable(spdlog)

############################################# nlohmann_json #####################################
if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty_src/nlohmann_json-src")
    message(STATUS "Found nlohmann_json in thirdparty_src, using existing download")
    FetchContent_Declare(nlohmann_json SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty_src/nlohmann_json-src")
else()
    message(STATUS "nlohmann_json not found, fetching from GitHub...")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/thirdparty_src/nlohmann_json-src
    )
endif()
FetchContent_MakeAvailable(nlohmann_json)


# Required dependencies
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# ============================================================================
# common: shared foundation utilities
# ============================================================================
file(GLOB_RECURSE COMMON_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/common/*.cc
)

add_library(prosophor_common STATIC
    ${COMMON_SOURCES}
)

target_include_directories(prosophor_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_definitions(prosophor_common PUBLIC
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    PROSOPHOR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(prosophor_common
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# ============================================================================
# platform: cross-platform OS abstraction
# ============================================================================
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

add_library(prosophor_platform STATIC
    ${PLATFORM_SOURCES}
)

target_include_directories(prosophor_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_definitions(prosophor_platform PUBLIC
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(prosophor_platform
    PUBLIC
        prosophor_common
)

if(WIN32)
    target_link_libraries(prosophor_platform PUBLIC ws2_32 bcrypt)
endif()

# ============================================================================
# prosophor_core: pure business logic, no UI/SDL dependency
# Used by both TUI and SDL builds
# ============================================================================
file(GLOB_RECURSE CORE_SOURCES_ALL
    ${CMAKE_CURRENT_LIST_DIR}/prosophor_core/*.cc
)

add_library(prosophor_core STATIC
    ${CORE_SOURCES_ALL}
)

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
        prosophor_platform
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    PRIVATE
        CURL::libcurl
        OpenSSL::SSL
        OpenSSL::Crypto
)

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


if(WIN32)
    target_link_libraries(prosophor_core PUBLIC ws2_32 bcrypt)
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
    add_executable(prosophor
        main.cc
        ${GUI_SOURCES}
        ${COMPONENTS_SOURCES}
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

