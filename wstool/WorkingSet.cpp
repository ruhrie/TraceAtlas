#include "WorkingSet.h"
#include "MemoryTool.h"
using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;
    //uint64_t inputMapSize = 0;
    //uint64_t internalMapSize = 0;
    //uint64_t BBCount = 0;

    map<uint64_t,KernelWorkingSet> KernelWorkingSetMap;
    //internal address struct vector: to store the address structs of birth and death time
    //vector<InternaladdressLiving> internalAddressLivingVec;
    //internal address index map: to speed up the addresses seaching in the internal address struct vector
   // map<uint64_t, uint64_t> internalAddressIndexMap;
    //input address index set: to store the input address indexes
    //set<uint64_t> inputAddressIndexSet;
    //output address index set: to store the output address indexes
    //set<uint64_t> outputAddressIndexSet;
    // FirstStore is called when we know that the address appears for the first time, then:
    // 1.Update address index map
    // 2.Construct the address structs of birth and death time
    // “fromStore” is a flag to indicate that if the address first appears from a load or a store instruction
    void firstStore(uint64_t addrIndex, int64_t t, bool fromStore, uint64_t kernelIndex)
    {
        cout<<"11111111"<<endl;
        // birth from a store inst
        if (fromStore)
        {
            cout<<"1222221"<<endl;
            InternaladdressLiving internalAddress = {.address = addrIndex, .birthTime = t, .deathTime = -1};
            //store the address into output set temporally
            KernelWorkingSetMap[kernelIndex].outputAddressIndexSet.insert(addrIndex);
            //update the map and the vector, if the address has never shown before
            KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[addrIndex] = KernelWorkingSetMap[kernelIndex].internalMapSize;
            KernelWorkingSetMap[kernelIndex].internalAddressLivingVec.push_back(internalAddress);
            KernelWorkingSetMap[kernelIndex].internalMapSize++;
        }
        // birth from a load inst
        else
        {
            cout<<"33333"<<endl;
            KernelWorkingSetMap[kernelIndex].inputAddressIndexSet.insert(addrIndex);
            KernelWorkingSetMap[kernelIndex].inputMapSize++;
        }
    }
    void Process(string &key, string &value)
    {    
        uint64_t addressIndex;
        cout<<"key:"<< key<<endl;
        cout<<"value:"<< value<<endl;
        cout<<"size:"<< KernelWorkingSetMap.size()<<endl;
        if (key == "BBEnter")
        {
            for(auto it: kernelMap)
            {
                if (it.second.find(stoul(value, nullptr, 0))!= it.second.end())
                {
                    BBCount[it.first]++;
                }
            }
            //cout<< "BBID:"<<stoul(value, nullptr, 0)<<endl;
        }
        if (key == "BBExit")
        {
            for(auto it: kernelMap)
            {
                if (it.second.find(stoul(value, nullptr, 0))!= it.second.end())
                {
                    BBCount[it.first]--;
                }
            }
        }
        for (auto &it: kernelMap)
        {
            if (BBCount[it.first] > 0)
            {
                if (key == "StoreAddress")
                {
                    addressIndex = stoul(value, nullptr, 0);
                    firstStore(addressIndex, timing, true, it.first);
                    timing++;
                }
                else if (key == "LoadAddress")
                {
                    addressIndex = stoul(value, nullptr, 0);
                    //Update the death time in address struct if the address is already in internal address vector
                    if (KernelWorkingSetMap[it.first].internalAddressIndexMap.find(addressIndex) != KernelWorkingSetMap[it.first].internalAddressIndexMap.end())
                    {
                        KernelWorkingSetMap[it.first].internalAddressLivingVec[KernelWorkingSetMap[it.first].internalAddressIndexMap[addressIndex]].deathTime = timing;
                        //remove the address from output set, if there is a load corresponding to a store
                        KernelWorkingSetMap[it.first].outputAddressIndexSet.erase(addressIndex);
                    }
                    else if (KernelWorkingSetMap[it.first].inputAddressIndexSet.find(addressIndex) == KernelWorkingSetMap[it.first].inputAddressIndexSet.end())
                    {
                        firstStore(addressIndex, timing, false, it.first);
                    }
                    timing++;
                }
            }
        }
        
    }
} // namespace WorkingSet