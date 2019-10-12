
#include <string>
#include "EncodeDetect.hpp"

int main()
{
	char sourceFile[] = "./test.trc";
	DetectKernels(sourceFile, 0.95, 512, false);
	return 0;
}


