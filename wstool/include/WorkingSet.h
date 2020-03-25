#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;

namespace WorkingSet
{
    extern map< string,  vector<uint64_t> > virAddr;
    extern uint64_t inputSize;
    void Process(std::string &key, std::string &value);  
} 