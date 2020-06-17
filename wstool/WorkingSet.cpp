#include "WorkingSet.h"
#include "MemoryTool.h"
using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;
    uint64_t inputMapSize = 0;
    uint64_t internalMapSize = 0;
    uint64_t BBCount = 0;
    uint64_t maxinternal = 0;


    map<uint64_t, int64_t> AddrEndtimeMap;
    set <int64_t> EndTimeFirst;
    //internal address struct vector: to store the address structs of birth and death time
    vector<InternaladdressLiving> internalAddressLivingVec;
    //internal address index map: to speed up the addresses seaching in the internal address struct vector
    map<uint64_t, uint64_t> internalAddressIndexMap;
    //input address index set: to store the input address indexes
    set<uint64_t> inputAddressIndexSet;
    
    // FirstStore is called when we know that the address appears for the first time, then:
    // 1.Update address index map
    // 2.Construct the address structs of birth and death time
    // “fromStore” is a flag to indicate that if the address first appears from a load or a store instruction

    // 1. max size update 2. erase/update the vector 
    void SizeUpdate(uint64_t addrIndex, int64_t t)
    {
        set <int64_t> endTimeSet;
        for (auto it : internalAddressLivingVec)
        {
            if (it.deathTime > 0)
            {
                endTimeSet.insert(it.deathTime);
                while (it.birthTime > *(endTimeSet.begin()))
                {
                    endTimeSet.erase(endTimeSet.begin());
                }
                if (endTimeSet.size() > maxinternal)
                {
                    maxinternal = endTimeSet.size();
                }
            }
        }
    }
    void firstStore(uint64_t addrIndex, int64_t t, bool fromStore)
    {
        // birth from a store inst
        if (fromStore)
        {
            if (internalAddressIndexMap.find(addressIndex) == internalAddressIndexMap.end())
            {
                InternaladdressLiving internalAddress = {.address = addrIndex, .birthTime = t, .deathTime = -1};
                //update the map and the vector, if the address has never shown before
                internalAddressIndexMap[addrIndex] = internalMapSize;
                internalAddressLivingVec.push_back(internalAddress);
                internalMapSize++;
            }
            else if (internalAddressLivingVec[internalAddressIndexMap[addrIndex]].deathTime != -1)
            {
                SizeUpdate(addrIndex,t);
            }
            
        }
        // birth from a load inst

        else
        {
            inputAddressIndexSet.insert(addrIndex);
            inputMapSize++;
        }
    }
    void Process(string &key, string &value)
    {    
        uint64_t addressIndex;
        
        if (key == "BBEnter"  &&  kernelBlockValue.find(stoul(value, nullptr, 0))!= kernelBlockValue.end())
        {
            BBCount++;
        }
        if (key == "BBExit"  &&  kernelBlockValue.find(stoul(value, nullptr, 0))!= kernelBlockValue.end())
        {
            BBCount--;
        }
        if (BBCount > 0)
        {
            if (key == "StoreAddress")
            {
                addressIndex = stoul(value, nullptr, 0);
                firstStore(addressIndex, timing, true);
                timing++;
            }
            else if (key == "LoadAddress")
            {
                addressIndex = stoul(value, nullptr, 0);
                //Update the death time in address struct if the address is already in internal address vector
                if (internalAddressIndexMap.find(addressIndex) != internalAddressIndexMap.end())
                {
                    internalAddressLivingVec[internalAddressIndexMap[addressIndex]].deathTime = timing;
                }
                else if (inputAddressIndexSet.find(addressIndex) == inputAddressIndexSet.end())
                {
                    firstStore(addressIndex, timing, false);
                }
                timing++;
            }
        }
    }
    void ProcessFirst(string &key, string &value)
    {    
        uint64_t addressIndex;
        
        if (key == "BBEnter"  &&  kernelBlockValue.find(stoul(value, nullptr, 0))!= kernelBlockValue.end())
        {
            BBCount++;
        }
        if (key == "BBExit"  &&  kernelBlockValue.find(stoul(value, nullptr, 0))!= kernelBlockValue.end())
        {
            BBCount--;
        }
        if (BBCount > 0)
        {
            if (key == "StoreAddress")
            {
                addressIndex = stoul(value, nullptr, 0);
                if(AddrEndtimeMap.find(addressIndex) == AddrEndtimeMap.end())
                {
                    AddrEndtimeMap[addressIndex] = 1;
                }
                else if (AddrEndtimeMap[addressIndex] != 1)
                {
                    EndTimeFirst.insert(AddrEndtimeMap[addressIndex]);   
                }
                timing++;
            }
            else if (key == "LoadAddress")
            {
                addressIndex = stoul(value, nullptr, 0);
                if(AddrEndtimeMap.find(addressIndex) != AddrEndtimeMap.end())
                {
                    AddrEndtimeMap[addressIndex] = timing;
                }
                timing++;
            }
        }
    }
} // namespace WorkingSet