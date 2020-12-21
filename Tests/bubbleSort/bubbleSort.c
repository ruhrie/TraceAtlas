#include "Backend/BackendTrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
// initialized in stack of the memory
int *get_input(int n, int i)
{

    int *in = (int *)malloc(sizeof(int) * n);
    for (i = 0; i < n; i++)
    {
        in[i] = rand(); 
    }
    return in;
}

void bubblesort(int *ini, int*outi, int size,bool reverse)
{
    int swap;
    
    for (int i1 = 0; i1 < size; i1++)
    {

        for (int j = i1; j < size; j++) 
        {
            (((ini[i1] < ini[j]) && !reverse)||((ini[i1] > ini[j]) && reverse))&& (swap = ini[i1],*(ini + i1) = ini[j],*(ini + j) = swap );
        }
        outi[i1] = ini[i1];
    }
}

int main(int argc, char *argv[])
{
    int i,j;
    int *out = (int *)malloc(sizeof(int) * 1000);
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
    int *in = (int *)malloc(sizeof(int) * 1000);
    KernelEnter("randomInit");
    
    for (i = 0; i < 1000; i++)
    {
        in[i] = rand(); 
    }
    KernelExit("randomInit");
    


    
    KernelEnter("bubbleSort");
    bubblesort(in,out,SIZE,false);
    KernelExit("bubbleSort");

    printf("second sort\n");
    KernelEnter("bubbleSort");
    bubblesort(out,out,SIZE,true);
    KernelExit("bubbleSort");
    printf("third sort\n");
    KernelEnter("bubbleSort");
    bubblesort(out,out,SIZE,false);
    KernelExit("bubbleSort");
    printf("forth sort\n");
    KernelEnter("bubbleSort");
    bubblesort(out,out,SIZE,true);
    KernelExit("bubbleSort");


    // printf("\n second Sorting Done");
    free(in);
    return 0;
}
