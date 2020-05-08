#include "WorkingSet.h"

using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;
    uint64_t inputMapSize = 0;
    uint64_t internalMapSize = 0;
    uint64_t maxInternalSize = 0;
    uint64_t maxOutputSize = 0;
    //address living vector: to store the address structs of birth and death time
    vector<InternaladdressLiving> internalAddressLivingVec;
    map<uint64_t, uint64_t> internalAddressIndexMap;
    // key map to speed up the address seaching, because there are many same addresses in the address living vector, we need to locate the latest one.
    set<uint64_t> inputAddressIndexSet;
    set<uint64_t> outputSet; //using this set of output addresses in case of duplicated
    

    // FirstStore is called when we know that the address appears for the first time, then:
    // 1.Update key map with the live addresses
    // 2.Construct the address structs of birth and death time
    // Here “fromStore” is a flag to indicate that if the address first appears from a load or a store
    void firstStore(uint64_t addrIndex, int64_t t, bool fromStore)
    {
                // birth from a store inst
        if (fromStore)
        {
            InternaladdressLiving internalAddress = {.address = addrIndex, .brithTime = t, .deathTime = -1};
            if (internalAddressIndexMap.find(addrIndex) == internalAddressIndexMap.end())
            {
                //update the map and the vector
                internalAddressIndexMap[addrIndex] = internalMapSize;
                internalAddressLivingVec.push_back(internalAddress);

                // we need to increase this size even if the address is duplicate, because the birth time is different which
                // indicates two life periods of this addr
                internalMapSize++;
            }
            else
            {
                if (internalAddressLivingVec[internalAddressIndexMap[addrIndex]].deathTime != -1)
                {
                    internalAddressIndexMap[addrIndex] = internalMapSize;
                    internalAddressLivingVec.push_back(internalAddress);
                    internalMapSize++;
                }
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
        int64_t intervalDistance = 1000;
        if (key == "StoreAddress")
        {
            addressIndex = stoul(value, nullptr, 0);
            firstStore(addressIndex, timing, true);
            timing++;
        }
        else if (key == "LoadAddress")
        {
            addressIndex = stoul(value, nullptr, 0);
            // Update the death time in address living struct:
            // The times when load instruction appears replaces the old death time.
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
        if (timing % intervalDistance == 0)
        {           
            set<int64_t> endTimeSet;
            uint64_t tempSize=0;
            if (!internalAddressLivingVec.empty())
            {               
                for (auto it : internalAddressLivingVec)
                {
                    //if the deathTime is "-1", it's an output address
                    if (it.deathTime == -1 && outputSet.find(it.address) == outputSet.end())
                    {
                        outputSet.insert(it.address);
                    }        
                    if (it.deathTime > 0)
                    {
                        endTimeSet.insert(it.deathTime);
                        if (it.brithTime > *(endTimeSet.begin()))
                        {
                            endTimeSet.erase(endTimeSet.begin());
                        }
                        if (endTimeSet.size() > tempSize)
                        {
                            tempSize = endTimeSet.size();
                        }
                    }                    
                }
                if(tempSize > maxInternalSize)
                {
                    maxInternalSize = tempSize;
                }
            }
            
            internalAddressLivingVec.clear();
            internalAddressIndexMap.clear();
            internalMapSize = 0;
        }
    maxOutputSize = outputSet.size();
    }
} // namespace WorkingSet