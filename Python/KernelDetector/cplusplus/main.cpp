
#include <vector>
#include <set>
#include "EncodeDetect.h"
#include "EncodeExtract.h"
#include <string>

int main()
{
    char sourceFile[] = "./testing/kalman_0_0.trc";
    std::vector< std::set< int > > type1Kernels = DetectKernels(sourceFile, 0.95, 512, false);
    std::cout << "Detected " << type1Kernels.size() << " type 1 kernels." << std::endl;
    std::map< int, std::vector< int > > type2Kernels = ExtractKernels(sourceFile, type1Kernels, false);
    std::cout << "Detected " << type2Kernels.size() << " type 2 kernels." << std::endl;

    /*for( auto entry : kernels )
    {
        for( auto index : entry )
        {
            std::cout << index << " , ";
        }
        std::cout << "\n\n";
    }*/
    return 0;
}
