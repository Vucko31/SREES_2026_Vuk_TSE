#pragma once

#ifdef MU_WINDOWS
    #ifdef MATRIX_EXPORTS
        #define MATRIX_API __declspec(dllexport)
    #else
        #define MATRIX_API __declspec(dllimport)
    #endif
#else
    #ifdef MATRIX_EXPORTS
        #define MATRIX_API __attribute__((visibility("default")))
    #else
        #define MATRIX_API
    #endif
#endif

#define MATRIX_EXTERN extern
