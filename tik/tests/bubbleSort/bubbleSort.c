#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "input.h"

int main(int argc, char* argv[])
{
	// for segfault example
	//int* foo = 0;
	int SIZE;
	if(argc >= 1)
	{
		SIZE = atoi(argv[1]);
	}
	else 
	{
		SIZE = 1000;
	}
	printf("\nSIZE = %d", SIZE);
	// allocate and read input from stdin
	int in[SIZE];
	get_input(SIZE, in);
	// bubble sort
	int swap;
	for(int i = 0; i < SIZE; i++)
	{
		for(int j = i; j < SIZE; j++)
		{
			if(in[i] > in[j]) 
			{ 
				swap  = in[i]; 
				in[i] = in[j]; 
				in[j] = swap; 
			}
		}
	}
	/*if(*foo)
	{
		return 0;
	}*/
	printf("\nSorting Done");
	return 0;
}



