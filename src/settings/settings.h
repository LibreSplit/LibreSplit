#pragma once

#include "definitions.h"

#include <jansson.h>

#include <stdbool.h>

extern AppConfig cfg;

#define CFG_SET_STR(a, b)             \
    do {                              \
        strncpy(a, b, sizeof(a) - 1); \
        a[sizeof(a) - 1] = '\0';      \
    } while (0)

bool config_init(void);
bool config_save(void);
