cmake_minimum_required(VERSION 3.20)

# Set Windows version to Windows 10 (required by cpp-httplib)
# and math constants for MinGW
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00 _USE_MATH_DEFINES M_PI=3.14159265358979323846)
endif()

# Set default version if PACKAGE_VERSION not provided
if(NOT DEFINED PACKAGE_VERSION)
    set(PACKAGE_VERSION "1.0.0")
endif()

# Remove 'v' prefix if present (e.g., v0.2.0 -> 0.2.0)
string(REGEX REPLACE "^v" "" PACKAGE_VERSION_CLEAN ${PACKAGE_VERSION})

project(prosophor VERSION ${PACKAGE_VERSION_CLEAN} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Suppress CMP0148 warning from third-party deps (FindPythonInterp removed)
if(POLICY CMP0148)
    cmake_policy(SET CMP0148 OLD)
endif()

# Set install prefix to "install" directory in build folder
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install prefix" FORCE)
endif()

include(FetchContent)

############################################# llama.cpp (optional) #####################################
option(PROSOPHOR_BUILD_LLAMA "Build llama.cpp for local model inference (llama-server)" ON)

############################################# Debug borders (optional) #####################################

if(PROSOPHOR_BUILD_LLAMA)
    if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty_src/llamacpp-src")
        message(STATUS "Found llama.cpp in thirdparty_src, using existing download")
        FetchContent_Declare(llamacpp SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty_src/llamacpp-src")
    else()
        message(STATUS "llama.cpp not found, fetching from GitHub...")
        FetchContent_Declare(
            llamacpp
            GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
            GIT_TAG master
            GIT_SHALLOW TRUE
            GIT_SUBMODULES ""
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/thirdparty_src/llamacpp-src
        )
    endif()

    set(LLAMA_BUILD_COMMON  ON  CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TOOLS   ON  CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER  ON  CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(llamacpp)
endif()

add_subdirectory(main_src)



# Install configuration files, roles, skills, and workspace
# Install to ~/.prosophor (home directory) on all platforms
if(WIN32)
    # On Windows, use USERPROFILE environment variable
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}/.prosophor" PROSOPHOR_INSTALL_DIR)
else()
    # On Unix-like systems, use HOME environment variable
    file(TO_CMAKE_PATH "$ENV{HOME}/.prosophor" PROSOPHOR_INSTALL_DIR)
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/config/.prosophor/ DESTINATION ${PROSOPHOR_INSTALL_DIR})

# Install assets directory to bin/assets (relative to executable)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION bin/assets)

# Also install config to bin/.prosophor/ for portable use (fallback path)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/config/.prosophor/ DESTINATION bin/.prosophor)

# ============================================================================
# Runtime DLL bundling (MinGW on Windows)
# ============================================================================
if(WIN32 AND MINGW)
    # Use compiler's directory to find MinGW DLLs (more reliable than MINGW_PREFIX env var)
    get_filename_component(MINGW_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)

    # All runtime DLLs (prosophor direct deps + libcurl transitive deps)
    install(FILES
        "${MINGW_BIN_DIR}/libcurl-4.dll"
        "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
        "${MINGW_BIN_DIR}/libstdc++-6.dll"
        "${MINGW_BIN_DIR}/libwinpthread-1.dll"
        "${MINGW_BIN_DIR}/zlib1.dll"
        "${MINGW_BIN_DIR}/libcrypto-3-x64.dll"
        "${MINGW_BIN_DIR}/libssl-3-x64.dll"
        "${MINGW_BIN_DIR}/libbrotlicommon.dll"
        "${MINGW_BIN_DIR}/libbrotlidec.dll"
        "${MINGW_BIN_DIR}/libiconv-2.dll"
        "${MINGW_BIN_DIR}/libidn2-0.dll"
        "${MINGW_BIN_DIR}/libintl-8.dll"
        "${MINGW_BIN_DIR}/libnghttp2-14.dll"
        "${MINGW_BIN_DIR}/libnghttp3-9.dll"
        "${MINGW_BIN_DIR}/libngtcp2-16.dll"
        "${MINGW_BIN_DIR}/libngtcp2_crypto_ossl-0.dll"
        "${MINGW_BIN_DIR}/libpsl-5.dll"
        "${MINGW_BIN_DIR}/libssh2-1.dll"
        "${MINGW_BIN_DIR}/libunistring-5.dll"
        "${MINGW_BIN_DIR}/libzstd.dll"
        DESTINATION bin
    )

    # CA certificate bundle for SSL verification
    install(FILES
        "${MINGW_BIN_DIR}/../etc/ssl/certs/ca-bundle.crt"
        DESTINATION bin
    )
endif()

# ============================================================================
# CPack packaging
# ============================================================================
set(CPACK_PACKAGE_NAME "Prosophor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AI Desktop Companion")
set(CPACK_PACKAGE_VENDOR "Prosophor Contributors")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Prosophor")
set(CPACK_PACKAGE_EXECUTABLES "prosophor" "Prosophor")

if(WIN32)
    # Try NSIS first, fall back to ZIP (which requires no external tools)
    find_program(NSIS_MAKENSIS makensis)
    if(NSIS_MAKENSIS)
        set(CPACK_GENERATOR "NSIS")
        set(CPACK_NSIS_DISPLAY_NAME "Prosophor")
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
        set(CPACK_NSIS_CREATE_ICONS_EXTRA
            "CreateShortCut '$SMPROGRAMS\\\\Prosophor\\\\Prosophor.lnk' '$INSTDIR\\\\bin\\\\prosophor.exe'")
        set(CPACK_NSIS_DELETE_ICONS_EXTRA
            "Delete '$SMPROGRAMS\\\\Prosophor\\\\Prosophor.lnk'")
        set(CPACK_NSIS_INSTALLED_FILES_DISTRIBUTION OFF)
    else()
        message(STATUS "NSIS not found, using ZIP package")
        set(CPACK_GENERATOR "ZIP")
    endif()
endif()

include(CPack)