#pragma once

typedef struct{
	void *next;
	int key;
	char *exec;
}Y_KEY_TOOL;

Y_KEY_TOOL *y_key_tools_load(void);
void y_key_tools_free(Y_KEY_TOOL *kt);
int y_key_tools_run(Y_KEY_TOOL *kt,int key);
