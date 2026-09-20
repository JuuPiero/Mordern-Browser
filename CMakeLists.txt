cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{DEVKITPRO})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DEVKITPRO}/cmake/3DS.cmake" CACHE PATH "toolchain file" FORCE)
endif()

project(MordernBrowser LANGUAGES C CXX)

if(NOT NINTENDO_3DS)
    message(FATAL_ERROR
        "MordernBrowser targets the Nintendo 3DS and must be configured with "
        "devkitPro's toolchain, e.g.:\n"
        "  cmake -B build -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- devkitPro portlibs (precompiled for 3DS, installed via pacman) --------
# TLS/HTTP for page fetching, libpng/libjpeg for images, freetype for text.
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(MBEDTLS REQUIRED IMPORTED_TARGET mbedtls mbedx509 mbedcrypto)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng16)
pkg_check_modules(JPEG REQUIRED IMPORTED_TARGET libjpeg)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

# --- vendored, trimmed third-party sources ----------------------------------
add_subdirectory(Vendor/lexbor)
add_subdirectory(Vendor/quickjs)
add_subdirectory(Vendor/litehtml)

# --- MordernBrowser ----------------------------------------------------------
file(GLOB_RECURSE MORDERNBROWSER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
)

add_executable(MordernBrowser ${MORDERNBROWSER_SOURCES})

target_include_directories(MordernBrowser PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_link_libraries(MordernBrowser PRIVATE
    citro2d
    citro3d
    litehtml
    lexbor
    quickjs
    PkgConfig::CURL
    PkgConfig::MBEDTLS
    PkgConfig::PNG
    PkgConfig::JPEG
    PkgConfig::FREETYPE
    PkgConfig::ZLIB
)

ctr_create_3dsx(MordernBrowser
    NAME "MordernBrowser"
    DESCRIPTION "A modern-ish web browser for the Nintendo 3DS"
    ROMFS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/romfs"
)
