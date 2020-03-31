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
    extern map<string, vector<int64_t>> virAddr;
    extern uint64_t inputSize;
    void Process(std::string &key, std::string &value);
} // namespace WorkingSet