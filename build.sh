mkdir build
git submodule init
git submodule update
./vcpkg/vcpkg install nlohmann-json
./vcpkg/vcpkg install spdlog
./vcpkg/vcpkg install indicators
cd build
cmake -DCMAKE_TOOLCHAIN_FILE="../vcpkg/scripts/buildsystems/vcpkg.cmake" -DCMAKE_CXX_COMPILER="/usr/bin/clang++-9" -DCMAKE_C_COMPILER="/usr/bin/clang-9"  ../
