#include <stdio.h>
#include <stdlib.h>
#include <time.h>
const int WIDTH = 1024;

void Recurse(int *input, int *output, int i)
{
    if (i != 0)
        output[i] = input[i - 1] + input[i] + input[i + 1];
    if (i != WIDTH - 1)
    {
        Recurse(input, output, ++i);
    }
}

int main()
{
    int *input = (int *)malloc(sizeof(int) * WIDTH);
    int *output = (int *)malloc(sizeof(int) * (WIDTH - 2));

    srand(time(NULL));
    //initialize the data
    for (int i = 0; i < WIDTH; i++)
    {
        input[i] = rand();
    }
    int j = 0;
    Recurse(input, output, j);

    printf("Success\n");
    return 0;
}