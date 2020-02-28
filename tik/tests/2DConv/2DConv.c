#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define WIDTH 1024
int main()
{
    int *input = malloc(sizeof(int) * WIDTH * WIDTH);
    int *output = malloc(sizeof(int) * (WIDTH - 1) * (WIDTH - 1));
    int a = 1;
    int b = -1;
    int c = -1;
    int d = 1;

    srand(time(NULL));
    //initialize the data
    for (int i = 0; i < WIDTH * WIDTH; i++)
    {
        input[i] = rand();
    }

    for (int y = 0; y < (WIDTH - 1); y++)
    {
        for (int x = 0; x < (WIDTH - 1); x++)
        {
            output[y * (WIDTH - 1) + x] = a * input[x + y * WIDTH] + b * input[x + 1 + y * WIDTH] + c * input[x + (y + 1) * WIDTH] + d * input[x + 1 + (y + 1) * WIDTH];
        }
    }

    printf("Success\n");
    return 0;
}