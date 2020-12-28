#include "Backend/BackendTrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int *get_input(int n)
{
    int *in = (int *)malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++)
    {
        *(in + i) = rand();
    }
    return in;
}

int main(int argc, char *argv[])
{
    int SIZE;
    if (argc > 1)
    {
        SIZE = atoi(argv[1]);
    }
    else
    {
        SIZE = 1000;
    }
    printf("\nSIZE = %d", SIZE);
    TraceAtlasKernelEnter("randInit");
    int *in = get_input(SIZE);
    TraceAtlasKernelExit("randInit");

    // bubble sort
    int swap;
    TraceAtlasKernelEnter("Bubblesort");
    for (int i = 0; i < SIZE; i++)
    {
        for (int j = i; j < SIZE; j++)
        {
            if (in[i] > in[j])
            {
                swap = in[i];
                in[i] = in[j];
                in[j] = swap;
            }
        }
    }
    TraceAtlasKernelExit("Bubblesort");
    printf("\nSorting Done");
    free(in);
    return 0;
}
