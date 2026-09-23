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

# Experiment 21's thermal/integrity logger needs the temperature-sensor driver.
# Declared above the registration because a component dependency has to be in
# idf_component_register's REQUIRES - not bolted on with target_link_libraries,
# which emits a bare `-l<component>` and fails at link (the mistake that left
# NEEDLE_KBENCH unbuildable). With the option OFF the requirement list is the
# same four components, so the shipping image cannot move.
option(NEEDLE_THERMAL_DIAG "Log die temperature and PSRAM integrity" OFF)
set(MAIN_REQUIRES needle esp_partition esp_timer json)
if(NEEDLE_THERMAL_DIAG)
    list(APPEND MAIN_REQUIRES esp_driver_tsens)
endif()

idf_component_register(SRCS "main.c" "router.c"
                       INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/generated"
                       REQUIRES ${MAIN_REQUIRES})

if(NEEDLE_THERMAL_DIAG)
    target_sources(${COMPONENT_LIB} PRIVATE "thermal_diag.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ND_THERMAL_DIAG=1)
    # Measured harness fact: with `main` declaring REQUIRES explicitly, adding
    # esp_driver_tsens there did NOT put its include dir on main's compile line
    # (verified twice from the failing ninja command). The component's library is
    # already on the link line because IDF links every component, so the only
    # thing missing is the header path. Diagnostic build only - never ON for a
    # measured or shipping image.
    get_property(ND_TSENS_DIR TARGET idf::esp_driver_tsens PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT ND_TSENS_DIR)
        set(ND_TSENS_DIR "${IDF_PATH}/components/esp_driver_tsens/include")
    endif()
    target_include_directories(${COMPONENT_LIB} PRIVATE ${ND_TSENS_DIR})
endif()

# Experiment 2 (MimiModel queue): the device-side kernel microbenchmark. Off by
# default; with it on, app_main measures the row walkers and idles instead of
# serving requests. Needs a fresh build directory (idf.py -B), like every other
# compile-time knob in this project.
option(NEEDLE_KBENCH "Build the CQ2 kernel microbenchmark into the app" OFF)
if(NEEDLE_KBENCH)
    # dot4_tie728.S is the Experiment 22 screen kernel (see the file header). It is
    # compiled only into a kbench image, so a shipping configure never places it in
    # IRAM and the accepted image cannot move.
    target_sources(${COMPONENT_LIB} PRIVATE "kbench.c" "dot4_tie728.S")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ND_KBENCH=1)

    # Experiment 29: which performance lane this image screens. A bare
    # `idf.py -DND_KB_LANE=2` only sets a CMake cache variable - it does NOT reach
    # the compile line - so forward it explicitly. Standing campaign rule: verify a
    # knob through compile_commands.json, never by the fact that -D was typed.
    set(ND_KB_LANE "1" CACHE STRING "E29 lane: 1 engram pair, 2 qkv+gate, 3 rows-per-call curve")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ND_KB_LANE=${ND_KB_LANE})
endif()
