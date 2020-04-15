#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>

using namespace std;

namespace WorkingSet
{
    extern int64_t timing;
    extern map<int64_t, vector<int64_t>> internalVirAddr;
    extern map<int64_t, vector<int64_t>> inputVirAddr;
    extern int64_t inputMapSize;
    extern int64_t internalMapSize;
    void Process(std::string &key, std::string &value);
} // namespace WorkingSet