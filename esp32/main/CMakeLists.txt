# ---------------------------------------------------------------------------
# The tool schema is generated into a header from the canonical JSON file, so
# there is a single source of truth shared with the host tools.
# ---------------------------------------------------------------------------
set(ND_SCHEMA_PATH "${CMAKE_CURRENT_LIST_DIR}/../../tools/demo-tools.json")

foreach(SCHEMA tools routes)
    if(SCHEMA STREQUAL "tools")
        set(ND_SCHEMA_PATH "${CMAKE_CURRENT_LIST_DIR}/../../tools/demo-tools.json")
    else()
        set(ND_SCHEMA_PATH "${CMAKE_CURRENT_LIST_DIR}/../../tools/model-routes.json")
    endif()
    file(READ "${ND_SCHEMA_PATH}" ND_SCHEMA_RAW)
    string(REGEX REPLACE "[\r\n]" "" ND_SCHEMA_RAW "${ND_SCHEMA_RAW}")
    string(STRIP "${ND_SCHEMA_RAW}" ND_SCHEMA_RAW)
    string(REPLACE "\\" "\\\\" ND_SCHEMA_ESC "${ND_SCHEMA_RAW}")
    string(REPLACE "\"" "\\\"" ND_SCHEMA_ESC "${ND_SCHEMA_ESC}")
    string(TOUPPER "${SCHEMA}" ND_SCHEMA_NAME)
    configure_file("${CMAKE_CURRENT_LIST_DIR}/tools_schema.h.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/generated/${SCHEMA}_schema.h" @ONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ND_SCHEMA_PATH}")
endforeach()

idf_component_register(SRCS "main.c" "router.c"
                       INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/generated"
                       REQUIRES needle esp_partition esp_timer json)

# Experiment 2 (MimiModel queue): the device-side kernel microbenchmark. Off by
# default; with it on, app_main measures the row walkers and idles instead of
# serving requests. Needs a fresh build directory (idf.py -B), like every other
# compile-time knob in this project.
option(NEEDLE_KBENCH "Build the CQ2 kernel microbenchmark into the app" OFF)
if(NEEDLE_KBENCH)
    target_sources(${COMPONENT_LIB} PRIVATE "kbench.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ND_KBENCH=1)
endif()
