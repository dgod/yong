#pragma once

// #define L_USE_COROUTINE		0
// #define L_USE_XML_DUMP		1

#if !defined(L_USE_COROUTINE)
	#if defined(__EMSCRIPTEN__)
		#define L_USE_COROUTINE		0
	#elif defined(__ANDROID__)
		#define L_USE_COROUTINE		0
	#elif !defined(_WIN32)
		#define L_USE_COROUTINE		1
	#endif
#endif

