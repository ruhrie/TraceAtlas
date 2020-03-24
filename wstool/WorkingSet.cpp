#include "WorkingSet.h"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>

using namespace std;

namespace WorkingSet
{
    int timing;
    map deAlias;
    map deAliasInput;
    void Process(std::string &key, std::string &value)
    {
        string action = key;
        string address = value;
        if (deAlias.count(address) != 0) && (key == "LoadAddress")
        {
            livingLoad(address,timing, 1);
        }            
        else if (deAliasInput.count(address) != 0) && (key == "LoadAddress")
        {
            livingLoad(address,timing, -1);
        }         
        else if (key == "LoadAddress")
        //start kernel
        {
            firstStore(address,timing, -1);            
        }          
        else if (key == "StoreAddress")
        {
            firstStore(address,timing,1);
        }            
    }