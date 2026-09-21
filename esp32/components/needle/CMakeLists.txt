idf_component_register(
    SRCS "../../../engine/src/nd_cact.c"
         "../../../engine/src/nd_quant.c"
         "../../../engine/src/nd_tokenizer.c"
         "../../../engine/src/nd_model.c"
         "../../../engine/src/nd_grammar.c"
         "../../../engine/src/nd_sample.c"
    INCLUDE_DIRS "../../../engine/include"
    REQUIRES esp_timer
)

# Print the tier span requested and the span the blob directory allows.
option(NEEDLE_TIER_TRACE "Print the requested tier span at open" OFF)
if(NEEDLE_TIER_TRACE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ND_TIER_TRACE=1)
endif()

option(NEEDLE_PROFILE "Measure inference phases" OFF)
if(NEEDLE_PROFILE)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ND_PROFILE)
endif()

# Experiment 2 (MimiModel queue): the handwritten TIE728 CQ2 row walkers. Measured
# on the device against the C walker, over Needle 3's own projection shapes and
# its real pair tables, the single-row kernel is 31% cheaper per row when both
# cores split the range and bit-exact with the C result (see .auto/ideas.md), so
# it is the default. The microbenchmark still turns it on for its own A/B.
option(NEEDLE_LUT2_ASM "Use the handwritten TIE728 CQ2 row walker" ON)

option(NEEDLE_KBENCH_ASM "Assemble the handwritten TIE728 CQ2 kernels for the bench" OFF)
if(NEEDLE_LUT2_ASM OR NEEDLE_KBENCH_ASM)
    target_sources(${COMPONENT_LIB} PRIVATE "../../../engine/src/lut2_tie728.S")
endif()
if(NEEDLE_LUT2_ASM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ND_LUT2_ASM=1)
endif()
if(NEEDLE_KBENCH_ASM)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ND_KBENCH_ASM=1)
endif()
