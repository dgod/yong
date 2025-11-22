#pragma once

typedef LArray Y_KEY_TOOL2;
Y_KEY_TOOL2 *y_key_tools2_load(void);
void y_key_tools2_free(Y_KEY_TOOL2 *kt);
bool y_key_tools2_run(Y_KEY_TOOL2 *kt,int key);

