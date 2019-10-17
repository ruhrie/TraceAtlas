
#include <string>
#include "EncodeDetect.h"

int main()
{
	char sourceFile[] = "./testing/test.trc";
	DetectKernels(sourceFile, 0.95, 512, false);
	return 0;
}


