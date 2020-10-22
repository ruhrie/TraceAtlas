#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int32_t b = -1;
uint64_t *markovResult;
extern int32_t MarkovBlockCount;

void MarkovInit()
{
    markovResult = (uint64_t *)calloc(MarkovBlockCount * MarkovBlockCount, sizeof(int32_t));
}
void MarkovIncrement(int32_t a)
{
    markovResult[a * MarkovBlockCount + b]++;
    b = a;
}
void MarkovExport()
{
    char *tfn = getenv("MARKOV_FILE");
    char *fileName;
    if (tfn != NULL)
    {
        fileName = tfn;
    }
    else
    {
        fileName = "markov.csv";
    }
    FILE *fp = fopen(fileName, "w");
    for (int i = 0; i < MarkovBlockCount; i++)
    {
        bool first = true;
        for (int j = 0; j < MarkovBlockCount; j++)
        {
            if (first)
            {
                first = false;
            }
            else
            {
                fprintf(fp, ",");
            }
            fprintf(fp, "%lu", markovResult[i * MarkovBlockCount + j]);
        }
        fprintf(fp, "%c", '\n');
    }
    //fclose(fp);
}
