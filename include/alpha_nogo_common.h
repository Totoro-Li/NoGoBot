//
// Created by Totoro on 2022/11/5.
//

#ifndef ALPHA_NOGO_COMMON_H
#define ALPHA_NOGO_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef BUILD_SHARED
#define ALPHA_NOGO_EXTERN __declspec(dllexport)
#else // BUILD_SHARED
#define ALPHA_NOGO_EXTERN
#endif // BUILD_SHARED
#else // _WIN32
#define ALPHA_NOGO_EXTERN
#endif // _WIN32

#include<stdbool.h>
// CFFI_DEF_START
typedef struct {
    int x;
    int y;
} Point;

typedef enum {
    ENEMY = -1,
    EMPTY = 0,
    SELF = 1
} BoardState;


typedef void (*BoardOpCB)(Point p, BoardState color);
int receive_all(BoardOpCB cb);
int receive_once(BoardOpCB cb);

// CFFI_DEF_END

#ifdef __cplusplus
}
#endif // __cplusplus
#endif //ALPHA_NOGO_COMMON_H
