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
