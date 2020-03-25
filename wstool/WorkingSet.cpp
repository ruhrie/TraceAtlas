#include "WorkingSet.h"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <sstream>
#include<iostream>

using namespace std;

namespace WorkingSet
{
    uint64_t timing = 0;
    map< uint64_t,  string> deAlias;
    map< string,  vector<uint64_t> > virAddr;
    uint64_t inputSize;
    void firstStore( string addr, uint64_t t, int op)
    {
        uint64_t addrIndex = stol(addr, 0, 0);
        string VirAddrIndex;
        stringstream ss;
        if (op > 0)
        {
            // might problem int()
            //printf ("firstStore op1 \n");           
            vector<uint64_t> virAddrLine(2);
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
            //printf ("firstStore op0");
            if (deAlias.count(addrIndex) == 0)
            {
                vector<uint64_t> virAddrLine(3);
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

    void livingLoad(string addr, uint64_t t)
    {
        uint64_t addrIndex = stol(addr, 0, 0);
        virAddr[deAlias[addrIndex]].push_back(t);
    }
        
    void Process( string &key,  string &value)
    {
        long int addrIndex = stol(value, 0, 0);
        string address = value;
        std::cout<<"key:"<< key << std::endl;
        std::cout<<"value:"<< value << std::endl;
        printf("addrIndex : %ld \n",addrIndex);
        if ((deAlias.count(addrIndex) != 0) && (key == "WSLoadAddress"))
        {
            printf ("11111 \n");
            livingLoad(address,timing);
        }                    
        else if (key == "WSLoadAddress")
        {
            printf ("2222 \n");
            firstStore(address,timing, -1);            
        }          
        else if (key == "WSStoreAddress")
        {
            printf ("3333 \n");
            firstStore(address,timing,1);
        }   
        timing++;         
    }
}