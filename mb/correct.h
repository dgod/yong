#pragma once

#include "llib.h"
#include "mb.h"

int correct_init(void);
void correct_destroy(void);
bool correct_enabled(void);
bool correct_run(char *s,struct y_mb *mb,int filter,int *count);

