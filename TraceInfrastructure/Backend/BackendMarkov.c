#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint64_t b;
uint64_t *markovResult;
bool markovInit = false;
extern uint64_t MarkovBlockCount;

void MarkovInit()
{
    markovResult = (uint64_t *)calloc(MarkovBlockCount * MarkovBlockCount, sizeof(uint64_t));
}
void MarkovIncrement(uint64_t a)
{
    if(markovInit)
    {
        markovResult[a * MarkovBlockCount + b]++;
    }    
    else
    {
        markovInit = true;
    }    
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
    for (uint64_t i = 0; i < MarkovBlockCount; i++)
    {
        bool first = true;
        for (uint64_t j = 0; j < MarkovBlockCount; j++)
        {
            if (first)
            {
                first = false;
                fprintf(fp, "%lu", markovResult[i * MarkovBlockCount + j]);
            }
            else
            {
                fprintf(fp, ",%lu", markovResult[i * MarkovBlockCount + j]);
            }
        }
        fprintf(fp, "%c", '\n');
    }
    fclose(fp);
}
