function(embed_web_file symbol infile outfile)
    file(READ "${infile}" hex HEX)
    if(hex STREQUAL "")
        set(body "0x00")
    else()
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," body "${hex}")
        string(REGEX REPLACE ",$" "" body "${body}")
    endif()
    file(APPEND "${outfile}" "const unsigned char ${symbol}[] = { ${body} };\n")
    file(APPEND "${outfile}" "const unsigned int ${symbol}_len = sizeof(${symbol});\n\n")
endfunction()

set(WEB_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${WEB_GEN_DIR})
set(WEB_ASSETS_C ${WEB_GEN_DIR}/web_assets.c)
set(WEB_ASSETS_H ${WEB_GEN_DIR}/web_assets.h)

file(WRITE ${WEB_ASSETS_H} "#ifndef WEB_ASSETS_H\n#define WEB_ASSETS_H\n")
file(APPEND ${WEB_ASSETS_H} "extern const unsigned char web_index_html[];\n")
file(APPEND ${WEB_ASSETS_H} "extern const unsigned int web_index_html_len;\n")
file(APPEND ${WEB_ASSETS_H} "extern const unsigned char web_style_css[];\n")
file(APPEND ${WEB_ASSETS_H} "extern const unsigned int web_style_css_len;\n")
file(APPEND ${WEB_ASSETS_H} "extern const unsigned char web_app_js[];\n")
file(APPEND ${WEB_ASSETS_H} "extern const unsigned int web_app_js_len;\n")
file(APPEND ${WEB_ASSETS_H} "#endif\n")

file(WRITE ${WEB_ASSETS_C} "#include \"web_assets.h\"\n\n")
embed_web_file(web_index_html ${CMAKE_SOURCE_DIR}/web/index.html ${WEB_ASSETS_C})
embed_web_file(web_style_css ${CMAKE_SOURCE_DIR}/web/style.css ${WEB_ASSETS_C})
embed_web_file(web_app_js ${CMAKE_SOURCE_DIR}/web/app.js ${WEB_ASSETS_C})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/web/index.html
    ${CMAKE_SOURCE_DIR}/web/style.css
    ${CMAKE_SOURCE_DIR}/web/app.js
)

add_library(module_web STATIC
    web.c
    ${WEB_ASSETS_C}
)
target_include_directories(module_web PUBLIC ${MODULES_INCLUDE_DIR} ${WEB_GEN_DIR})
target_link_libraries(module_web PRIVATE threadx netxduo platform_netx platform_bsp module_console module_power)
