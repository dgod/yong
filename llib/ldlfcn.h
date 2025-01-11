#pragma once

#ifdef _WIN32

#include <windows.h>

#define l_dlopen(x) LoadLibraryA(x)
#define l_dlsym(x,y) (void*)GetProcAddress(x, y)
#define l_dlclose(x) FreeLibrary(x)
#define l_defsym(x,y) (void*)GetProcAddress(GetModuleHandlA(x), y)

#else

#include <dlfcn.h>

#define l_dlopen(x) dlopen(x,RTLD_LAZY)
#define l_dlsym(x,y) dlsym(x,y)
#define l_dlclose(x) dlclose(x)
#define l_defsym(x) dlsym(RTLD_DEFAULT,x)

#endif
