#include "WorkingSet.h"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <sstream>

using namespace std;

namespace WorkingSet
{
    int timing;
    map<int,  string> deAlias;
    map< string,  vector<int> > virAddr;
    int inputSize;
    void firstStore( string addr, int t, int op)
    {
        int addrIndex = atoi(addr.c_str());
        string VirAddrIndex;
        stringstream ss;
        if (op > 0)
        {
            // might problem int()           
            vector<int> virAddrLine(2);
            virAddrLine[0] = addrIndex;
            virAddrLine[1] = t;
            
            ss << addr << "@"<<t;
            VirAddrIndex.append(ss.str());

            virAddr[VirAddrIndex] = virAddrLine;          
            if  (deAlias.count(addrIndex) == 0)
            {
                deAlias[addrIndex] = VirAddrIndex;
            }              
            else if (virAddr[deAlias[addrIndex]].size() < 3)
            {
                virAddr[deAlias[addrIndex]].push_back(t);
                deAlias[addrIndex] = VirAddrIndex;
            }                
            else
            {
                deAlias[addrIndex] = VirAddrIndex;
            }               
        }           
        else
        {
            if (deAlias.count(addrIndex) == 0)
            {
                vector<int> virAddrLine(3);
                virAddrLine[0] = addrIndex;
                virAddrLine[1] = -1;
                virAddrLine[2] = t;


                VirAddrIndex = addr;
                ss <<"@-1";
                VirAddrIndex.append(ss.str());
                
                virAddr[VirAddrIndex] = virAddrLine;
                inputSize += 1;
                deAlias[addrIndex] = VirAddrIndex;
            }
            else
            {
                virAddr[deAlias[addrIndex]].push_back(t);
            }                
        }        
    }

    void livingLoad(string addr, int t)
    {
        int addrIndex = atoi(addr.c_str());
        virAddr[deAlias[addrIndex]].push_back(t);
    }
        
    void Process( string &key,  string &value)
    {
        int addrIndex = atoi(value.c_str());
        string address = value;
        if ((deAlias.count(addrIndex) != 0) && (key == "LoadAddress"))
        {
            livingLoad(address,timing);
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
        timing++;         
    }
}