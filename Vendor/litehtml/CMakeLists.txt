# Vendored litehtml (https://github.com/litehtml/litehtml) + its bundled
# gumbo-parser HTML5 tokenizer. Docs, containers/ (reference renderers for
# cairo/wxwidgets/etc -- we write our own for citro2d, see
# Platform/N3DS/HtmlContainer), support/ demo apps, and gumbo's own codegen
# inputs (tag.in, char_ref.rl -- their generated .c output is what's kept)
# were all dropped. Only litehtml itself does layout/CSS; MordernBrowser
# supplies just the drawing backend.

file(GLOB LITEHTML_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB GUMBO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gumbo/*.c"
)

add_library(litehtml STATIC ${LITEHTML_SOURCES} ${GUMBO_SOURCES})

set_target_properties(litehtml PROPERTIES
    CXX_STANDARD 17
    C_STANDARD 99
)

# PUBLIC: consumers (Platform/N3DS/HtmlContainer) write #include <litehtml/...>.
target_include_directories(litehtml PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# PRIVATE: litehtml's own .cpp files use flat #include "document.h" (no
# "litehtml/" prefix), and gumbo's sources likewise use flat #include
# "gumbo.h" / "attribute.h".
target_include_directories(litehtml PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include/litehtml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gumbo/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gumbo/include/gumbo"
)
