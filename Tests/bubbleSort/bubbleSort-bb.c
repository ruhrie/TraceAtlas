#include "Backend/BackendTrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>


// void bubblesort(int *in, int*out, int size,bool reverse)
// {
//     int swap;  
//     for (int i = 0; i < size; i++)
//     {
//         for (int j = i1; j < size; j++) 
//         {
//             if(((in[i] < out[j]) && !reverse)||((in[i] > in[j]) && reverse))
//             {
//                 swap = in[i];
//                 in[i] = in[j];
//                 in[j] = swap;
//             }
//         }
//         out[i] = in[i];
//     }
// }


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
        printf("out : %d \n",outi[i1]); 
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
        SIZE = 10; 
    }
    
    printf("\nSIZE = %d", SIZE);
    int *in = (int *)malloc(sizeof(int) * 1000);
    
    KernelEnter("randomInit");  
    for (i = 0; i < SIZE; i++)
    {
        in[i] = rand()%20;
        printf("in : %d \n",in[i]); 
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
