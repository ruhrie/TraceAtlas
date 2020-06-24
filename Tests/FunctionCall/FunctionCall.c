#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define WIDTH 1024

void Kernel0(int *output, int *input)
{
    output[0] = abs(input[0]);
}

int Kernel1(int input)
{
    return input * 3 - 2;
}

void Kernel2(const int *input, int *output)
{
    for (int i = 0; i < WIDTH; i++)
    {
        output[i] = input[i] * -1;
    }
}

int main()
{
    int *buf0 = malloc(sizeof(int) * WIDTH);
    int *buf1 = malloc(sizeof(int) * WIDTH);
    int *buf2 = malloc(sizeof(int) * WIDTH);
    int *buf3 = malloc(sizeof(int) * WIDTH);

    srand(time(NULL));
    //initialize the data
    for (int i = 0; i < WIDTH; i++)
    {
        buf0[i] = rand();
    }

    for (int i = 0; i < WIDTH; i++)
    {
        Kernel0(&(buf1[i]), &(buf0[i]));
    }

    for (int i = 0; i < WIDTH; i++)
    {
        buf2[i] = Kernel1(buf0[i]);
    }

    Kernel2(buf2, buf3);

    printf("Success\n");
    return 0;
}