/**
 * @file	input.h
 *
 * @author 	Ben Willis
 *
 * @brief	This file declares the functions defined in input.c.
 *
 */

#ifndef 	INPUT_H
#define 	INPUT_H

/**
 * @function 	read_number
 *
 * @brief 	This function will read a number from the input text file. 
 * 		It will read until it hits either a newline, tab or whitespace.
 *
 * @param[in] 	void
 * 
 * @retval	int 		Returns the number that was read from the input.
 *
 */
int read_number(void);

/**
 * @function 	get_input
 *
 * @brief	This file is a script for reading all vertices from the input text file.
 *
 * @param[in] 	int n 			 	Number of inputs to be read.
 * @param[in] 	int in[n]		 	Array to read all the numbers into.
 *
 */
void get_input(int n, int* in);


#endif

