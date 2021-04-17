#include <stdio.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include"math.h"
#include "Backend/BackendTrace.h"
double time[256] =
{0, 0.1000, 0.2000, 0.3000, 0.4000, 0.5000, 0.6000, 0.7000, 0.8000, 0.9000, 1.0000, 1.1000, 1.2000,
1.3000, 1.4000, 1.5000, 1.6000, 1.7000, 1.8000, 1.9000, 2.0000, 2.1000, 2.2000, 2.3000, 2.4000, 2.5000,
2.6000, 2.7000, 2.8000, 2.9000, 3.0000, 3.1000, 3.2000, 3.3000, 3.4000, 3.5000, 3.6000, 3.7000, 3.8000,
3.9000, 4.0000, 4.1000, 4.2000, 4.3000, 4.4000, 4.5000, 4.6000, 4.7000, 4.8000, 4.9000, 5.0000, 5.1000,
5.2000, 5.3000, 5.4000, 5.5000, 5.6000, 5.7000, 5.8000, 5.9000, 6.0000, 6.1000, 6.2000, 6.3000, 6.4000,
6.5000, 6.6000, 6.7000, 6.8000, 6.9000, 7.0000, 7.1000, 7.2000, 7.3000, 7.4000, 7.5000, 7.6000, 7.7000,
7.8000, 7.9000, 8.0000, 8.1000, 8.2000, 8.3000, 8.4000, 8.5000, 8.6000, 8.7000, 8.8000, 8.9000, 9.0000,
9.1000, 9.2000, 9.3000, 9.4000, 9.5000, 9.6000, 9.7000, 9.8000, 9.9000, 10.0000};

double received[512] =
{0.0000, 0.0000,  0.0000, 0.0000,  0.0000, 0.0000,  0.0000,  0.0000,   0.0000,  0.0000,   0.0000,  0.0000,
0.0000, 0.0000,  0.0000, 0.0000,  0.0000, 0.0000,  0.0000,  0.0000,   0.0000,  0.0000,   0.0000,  0.0000,
0.0000, 0.0000,  0.0000, 0.0000,  0.0000, 0.0000,  0.0000,  0.0000,   0.0000,  0.0000,   0.0000,  0.0000,
0.0000, 0.0000,  0.0000, 0.0000,  0.0000, 0.0000,  0.0000,  0.0000,   0.0000,  0.0000,   0.0000,  0.0000,
0.0000, 0.0000,  0.0000, 0.0000,  0.0000, 0.0000,  0.0000,  0.0000,   0.0000,  0.0000,   0.0000,  0.0000,
0.0000, 0.0000,  0.0000, 0.0000,  0.0000, 0.0000,  0.0000,  0.0000,   0.0000,  0.0000,   0.0000,  0.0000,
0.0000, 0.0000,  0.0000, 0.0000,  0.0000, 0.0000,  0.0000,  0.0000,   0.0000,  0.0000,   0.0000,  0.0000,
0.0000, 0.0000,  0.0000, 0.0000,  0.0000, 0.0000,  0.0000,  0.0000,   0.0000,  0.0000,   0.0000,  0.0000,
0.0000, 0.0000,  0.0000, 0.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000,   1.0000,  0.0000,
1.0000, 0.0000,  1.0000, 0.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000,   1.0000,  0.0000,
1.0000, 0.0000,  1.0000, 0.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000,   1.0000,  0.0000,
1.0000, 0.0000,  1.0000, 0.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000,   1.0000,  0.0000,
1.0000, 0.0000,  1.0000, 0.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000,   1.0000,  0.0000,
1.0000, 0.0000,  1.0000, 0.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000,   1.0000,  0.0000,
1.0000, 0.0000,  1.0000, 0.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000,   1.0000,  0.0000,
1.0000, 0.0000,  1.0000, 0.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000,   1.0000,  0.0000,
1.0000, 0.0000,  1.0000, 1.0000,  1.0000, 0.0000,  1.0000,  0.0000,   1.0000,  0.0000 };



int main(int argc, char *argv[])
{
    KernelEnter("randomInit");
	const size_t n_samples = 256;
	const double T = 0.000512;
	const double B = 500000;
	const double sampling_rate = 1000;
	const size_t dft_size = 2 * n_samples-1;
	double *dftMatrix = malloc(2* dft_size * dft_size * sizeof(double));
	FILE *fp;

	for (size_t i = 0; i < dft_size * dft_size * 2; i+=2)
	{
		dftMatrix[i] = cos(2*M_PI/dft_size*i);
		dftMatrix[i + 1] = -sin(2 * M_PI / dft_size * i);
	}


	double lag;
	double *corr = malloc( (2*(2*n_samples - 1)) * sizeof(double));
	double* gen_wave = malloc(2 * n_samples * sizeof(double));

	for (size_t i = 0; i < 2 * n_samples; i += 2)
	{
		gen_wave[i] = cos(M_PI * B / T * pow(time[i / 2], 2));
		gen_wave[i + 1] = sin(M_PI * B / T * pow(time[i / 2], 2));
	}
	//Add code for zero-padding, to make sure signals are of same length

	size_t len = 2 * n_samples - 1;

	double* c = malloc(2 * len * sizeof(double));
	double* d = malloc(2 * len * sizeof(double));

	size_t x_count = 0;
	size_t y_count = 0;
    KernelExit("randomInit");

    KernelEnter("k1");
	for (size_t i = 0; i < 2 * len; i += 2)
	{
		if (i / 2 > n_samples - 1)
		{
			c[i] = gen_wave[x_count];
			c[i + 1] = gen_wave[x_count + 1];
			x_count += 2;
		}
		else
		{
			c[i] = 0;
			c[i + 1] = 0;
		}

		if (i > n_samples)
		{
			d[i] = 0;
			d[i + 1] = 0;
		}
		else
		{
			d[i] = received[y_count];
			d[i + 1] = received[y_count + 1];
			y_count += 2;
		}

	}
    KernelExit("k1");
	double* X1 = malloc(2 * len * sizeof(double));
	double* X2 = malloc(2 * len * sizeof(double));
	double* corr_freq = malloc(2 * len * sizeof(double));
	int row;
	int column;
    KernelEnter("k2");
	for (size_t i = 0; i < dft_size * dft_size *2; i += 2)
	{
		row = i /(dft_size *2);
		column = i % (dft_size *2);
		X1[2*row] += dftMatrix[i] * c[column];
		X1[2*row+1] += dftMatrix[i+1] * c[column+1];
	}
    KernelExit("k2");

    KernelEnter("k3");
	for (size_t i = 0; i < dft_size * dft_size * 2; i += 2)
	{
		row = i / (dft_size *2);
		column = i % (dft_size *2);
		X2[2 * row] += dftMatrix[i] * d[column];
		X2[2 * row + 1] += dftMatrix[i + 1] * d[column + 1];
	}
    KernelExit("k3");

    KernelEnter("k4");
	for (size_t i = 0; i < 2 * len; i += 2)
	{
		corr_freq[i] = (X1[i] * X2[i]) + (X1[i + 1] * X2[i + 1]);
		corr_freq[i + 1] = (X1[i + 1] * X2[i]) - (X1[i] * X2[i + 1]);
	}
    KernelExit("k4");

    KernelEnter("k5");
	for (size_t i = 0; i < dft_size * dft_size * 2; i += 2)
	{
		row = i / (dft_size *2);
		column = i % (dft_size *2);
		corr[2 * row] += dftMatrix[i] * corr_freq[column]/511;
		corr[2 * row + 1] -= dftMatrix[i + 1] * corr_freq[column + 1] / 511;
	}
    KernelExit("k5");

    KernelEnter("k6");
	//Code to find maximum
	double max_corr = 0;
	double index = 0;
	for (size_t i = 0; i < 2 * (2 * n_samples - 1); i += 2)
	{
		// Only finding maximum of real part of correlation
		(corr[i] > max_corr) && (max_corr = corr[i], index = i / 2);

	}
	lag = (n_samples - index) / sampling_rate;
    KernelExit("k6");

	printf("Lag Value is: %lf \n", lag);
}
