# Vendored QuickJS core engine (https://bellard.org/quickjs/), trimmed to the
# interpreter itself: no qjs/qjsc CLI, no quickjs-libc (std/os modules), no
# tests, no docs. Source taken from Refs/dreamcart's already 3DS-ported copy,
# since it already carries the devkitARM portability fixes upstream QuickJS
# needs on this ABI (see patch.h and the -fpermissive/-Wno-* flags below).

add_library(quickjs STATIC
    cutils.c
    libregexp.c
    libunicode.c
    quickjs.c
)

target_include_directories(quickjs PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION="2021-03-27"
)

# QuickJS's int/int32_t pointer usage doesn't match the 3DS ABI (int32_t is
# `long` there); devkitARM's gcc-14 treats that as a hard error without this.
target_compile_options(quickjs PRIVATE
    -fpermissive
    -Wno-incompatible-pointer-types
    -Wno-implicit-function-declaration
    -Wno-int-conversion
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-function
    -Wno-format-truncation
    -Wno-array-bounds
    -Wno-sign-compare
)
