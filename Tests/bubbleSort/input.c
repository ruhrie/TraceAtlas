/**
 * @file	input.c
 *
 * @author 	Ben Willis
 *
 * @brief	This file defines the functions that will read in the vertices from Nd_dft_input.txt
 *		where N is the dimension of the dft to be performed. 
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "input.h"

int read_number(void)
{
	int length = 0;
	char string[25];
	char new_char;
	int newline = '\n';
	volatile int newlineequal = 100;
	do{
		scanf("%c", &new_char);
		string[length] = new_char;
		length++;
		newlineequal = new_char - newline;
	}while(newlineequal != 0);
	char refined_string[length];
	for(int i = 0; i < length; i++) { refined_string[i] = string[i]; }
	return atoi(string);
}

void get_input(int n,int* in)
{
	for(int i = 0; i < n; i++) {
		*(in + i) = read_number(); 
	}
}




