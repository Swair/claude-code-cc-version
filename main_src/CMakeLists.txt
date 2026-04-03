cmake_minimum_required(VERSION 3.20)
project(aicode_core VERSION 1.0.0 LANGUAGES CXX)


include(FetchContent)

# 使用 FetchContent 自动下载 spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

# 使用 FetchContent 自动下载 nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# Required dependencies
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Collect source files excluding platform-specific ones
file(GLOB_RECURSE CC_FILES_LIST
    ${CMAKE_CURRENT_LIST_DIR}/agents/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/cli/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/common/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/core/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/managers/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/mcp/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/providers/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/services/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/tools/*.cc
)

# Exclude platform-specific implementation files
list(FILTER CC_FILES_LIST EXCLUDE REGEX ".*/cli/input_handler\\.cc$")
list(FILTER CC_FILES_LIST EXCLUDE REGEX ".*/cli/input_handler_posix\\.cc$")
list(FILTER CC_FILES_LIST EXCLUDE REGEX ".*/cli/input_handler_win32\\.cc$")

# Add platform-specific input_handler based on the target platform
if(WIN32)
    list(APPEND CC_FILES_LIST
        ${CMAKE_CURRENT_LIST_DIR}/cli/input_handler_win32.cc
    )
else()
    list(APPEND CC_FILES_LIST
        ${CMAKE_CURRENT_LIST_DIR}/cli/input_handler_posix.cc
    )
endif()

add_library(aicode_core STATIC ${CC_FILES_LIST})

target_include_directories(aicode_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

target_compile_definitions(aicode_core PUBLIC
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    AICODE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(aicode_core
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    PRIVATE
        CURL::libcurl
        OpenSSL::SSL
        OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(aicode_core PUBLIC ws2_32 bcrypt)
endif()

add_executable(aicode main.cc)
target_link_libraries(aicode PRIVATE aicode_core)
target_include_directories(aicode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aicode_core PRIVATE -Wall -Wextra -O2)
    target_compile_options(aicode PRIVATE -Wall -Wextra -O2)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(aicode_core PRIVATE /W4 /Od /RTC1 /EHsc)
        target_compile_options(aicode PRIVATE /W4 /Od /RTC1 /EHsc)
    else()
        target_compile_options(aicode_core PRIVATE /W4 /O2 /EHsc)
        target_compile_options(aicode PRIVATE /W4 /O2 /EHsc)
    endif()
endif()

install(TARGETS aicode DESTINATION bin)
