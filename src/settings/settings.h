#pragma once

#include "definitions.h"
#include <jansson.h>

extern AppConfig cfg;

#define CFG_SET_STR(a, b) strncpy(a, b, sizeof(a) - 1);

bool config_init();
bool config_save();
