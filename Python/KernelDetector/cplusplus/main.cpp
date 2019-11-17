
#include <string>
#include "EncodeDetect.h"

int main()
{
	char sourceFile[] = "./testing/kalman_0_0.trc";
	DetectKernels(sourceFile, 0.95, 512, false);
	return 0;
}


