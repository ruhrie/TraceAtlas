#include <stdio.h>
#include <stdlib.h>

#define W 128
#define H 128

#define L 8
#define P 8

#define DATATYPE double

struct global_data_t
{
    DATATYPE a[W][H];
    DATATYPE b[W][H];
};
// global data structure used to communicate data between kernels
struct global_data_t karg;

int conv(DATATYPE a[W][H], DATATYPE b[W][H], int w, int h)
{
    DATATYPE conv = 0;
    for (int l = 0; l < L; l++)
    {
        for (int p = 0; p < P; p++)
        {
            conv += a[w + l][h + p] * b[w + l][h + p];
        }
    }
    return conv;
}

void kernelFunction0()
{
    for (int i = 0; i < W - L + 1; i++)
    {
        for (int j = 0; j < H - P + 1; j++)
        {
            karg.b[i][j] = conv(karg.a, karg.b, i, j);
        }
    }
}

void kernelFunction1()
{
    for (int i = 0; i < W - L + 1; i++)
    {
        for (int j = 0; j < H - P + 1; j++)
        {
            karg.b[i][j] = conv(karg.a, karg.b, i, j);
        }
    }
}

int main(int argc, char **argv)
{
    for (int i = 0; i < W; i++)
    {
        for (int j = 0; j < H; j++)
        {
            karg.a[i][j] = rand();
            karg.b[i][j] = rand();
        }
    }
    kernelFunction0();

    for (int i = 0; i < W; i++)
    {
        for (int j = 0; j < H; j++)
        {
            karg.a[i][j] = rand();
        }
    }
    kernelFunction1();
    return 0;
}