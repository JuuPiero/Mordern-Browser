# Vendored, trimmed lexbor (https://github.com/lexbor/lexbor).
#
# Only the modules MordernBrowser actually needs are kept: core, tag, ns,
# dom, html, css, selectors, style, encoding, punycode, unicode, url, plus
# the posix port (used by devkitARM's newlib). Docs, tests, examples,
# benchmarks, the wasm build, packaging, the "engine"/"utils" helper modules,
# and lexbor's own CMake feature-detection machinery were all dropped.

set(LEXBOR_MODULES
    core css dom encoding html ns punycode selectors style tag unicode url
)

set(LEXBOR_SOURCES "")
foreach(module ${LEXBOR_MODULES})
    file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/lexbor/${module}/*.c")
    list(APPEND LEXBOR_SOURCES ${module_sources})
endforeach()

list(APPEND LEXBOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/lexbor/ports/posix/lexbor/core/fs.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lexbor/ports/posix/lexbor/core/memory.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lexbor/ports/posix/lexbor/core/perf.c"
)

add_library(lexbor STATIC ${LEXBOR_SOURCES})

target_include_directories(lexbor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(lexbor PUBLIC
    LEXBOR_STATIC
)
