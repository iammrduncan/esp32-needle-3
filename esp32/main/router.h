#pragma once
#include <stddef.h>
#include <stdint.h>

void router_init(uint32_t layers, size_t model_bytes);
void router_dispatch(const char *generated);
void router_print_state(void);
