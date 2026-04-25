#pragma once

// #define L_USE_COROUTINE		0
// #define L_USE_XML_DUMP		1
// #define L_USE_EXPR_FUNC		0
// #define L_USE_GZ_EXTRACT		0
// #define L_USE_BSEARCH_R		0

#if !defined(L_USE_COROUTINE)
	#if defined(__EMSCRIPTEN__)
		#define L_USE_COROUTINE		0
	#elif defined(__ANDROID__)
		#define L_USE_COROUTINE		2
	#else
		#define L_USE_COROUTINE		1
	#endif
#endif

