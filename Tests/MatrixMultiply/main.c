
#include "Backend/BackendTrace.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef TRACING
#define TRACING 0
#endif

const int SIZE = 512;

int main()
{
    int(*in0)[SIZE] = malloc(SIZE * sizeof(int[SIZE][SIZE]));
    int(*in1)[SIZE] = malloc(SIZE * sizeof(int[SIZE][SIZE]));
    int(*out)[SIZE] = malloc(SIZE * sizeof(int[SIZE][SIZE]));

    for (int i = 0; i < SIZE; i++)
    {
        for (int j = 0; j < SIZE; j++)
        {
            in0[i][j] = rand();
            in1[i][j] = rand();
            out[i][j] = 0;
        }
    }

#if TRACING
    KernelEnter("MatrixMultiply,Outer");
#endif
    for (int i = 0; i < SIZE; i++)
    {
#if TRACING
        KernelEnter("MatrixMultiply,Inner");
#endif
        for (int j = 0; j < SIZE; j++)
        {
#if TRACING
            KernelEnter("MatrixMultiply,Mul");
#endif
            for (int k = 0; k < SIZE; k++)
            {
                out[i][j] += in0[i][k] * in1[k][j];
            }
#if TRACING
            KernelExit("MatrixMultiply,Mul");
#endif
        }
#if TRACING
        KernelExit("MatrixMultiply,Inner");
#endif
    }
#if TRACING
    KernelExit("MatrixMultiply,Outer");
#endif

    printf("Success.\n");
    return 0;
}
