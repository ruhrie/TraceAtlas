#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define WIDTH 1024
#define THREADS 16
void *KernelWorker()
{
    int *input = (int *)malloc(sizeof(int) * WIDTH);
    int *output = (int *)malloc(sizeof(int) * (WIDTH - 2));
    for (int i = 0; i < WIDTH; i++)
    {
        input[i] = rand();
    }

    for (int i = 1; i < WIDTH - 1; i++)
    {
        output[i - 1] = input[i - 1] + input[i] + input[i + 1];
    }
    return NULL;
}

int main()
{
    pthread_t threads[THREADS];
    srand(time(NULL));
    for (int i = 0; i < THREADS; i++)
    {
        pthread_create(&threads[i], NULL, KernelWorker, NULL);
    }

    for (int i = 0; i < THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }
    printf("Success\n");
    return 0;
}