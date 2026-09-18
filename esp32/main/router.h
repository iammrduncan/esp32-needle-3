#pragma once
#include <stddef.h>
#include <stdint.h>
#include "nd_grammar.h"

void router_init(uint32_t layers, size_t model_bytes);
void router_dispatch(const char *generated);
void router_select(const char *generated, const nd_grammar *routes);
void router_print_state(void);
