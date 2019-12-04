
#include <vector>
#include <set>
#include "EncodeDetect.h"
#include <string>

int main()
{
    char sourceFile[] = "./testing/kalman_0_0.trc";
    std::vector< std::set< int > > kernels = ExtractKernels(sourceFile, 0.95, 512, false);
    std::cout << "Detected " << kernels.size() << " type 1 kernels." << std::endl;
    for( auto entry : kernels )
    {
        for( auto index : entry )
        {
            std::cout << index << " , ";
        }
        std::cout << "\n\n";
    }
    return 0;
}
