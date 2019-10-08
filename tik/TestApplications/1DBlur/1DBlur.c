#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#define WIDTH 1024
int main()
{
    int *input = malloc(sizeof(int) * WIDTH);
    int *output = malloc(sizeof(int) * (WIDTH - 2));

    srand(time(NULL));
    //initialize the data
    for(int i = 0; i < WIDTH; i++)
    {
        input[i] = rand();
    }

    for(int i = 1; i < WIDTH - 1; i++)
    {
        output[i - 1] = input[i - 1] + input[i] + input[i + 1];
    }

    printf("Success\n");
    return 0;
}