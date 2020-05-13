#include "WorkingSet.h"

using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;
    uint64_t inputMapSize = 0;
    uint64_t internalMapSize = 0;
    //address living vector: to store the address structs of birth and death time
    vector<InternaladdressLiving> internalAddressLivingVec;
    map<uint64_t, uint64_t> internalAddressIndexMap;
    // key map to speed up the address seaching, because there are many same addresses in the address living vector, we need to locate the latest one.
    set<uint64_t> inputAddressIndexSet;
    set<uint64_t> outputAddressIndexSet;
    

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
            outputAddressIndexSet.insert(addrIndex);
            if (internalAddressIndexMap.find(addrIndex) == internalAddressIndexMap.end())
            {
                //update the map and the vector
                internalAddressIndexMap[addrIndex] = internalMapSize;
                internalAddressLivingVec.push_back(internalAddress);

                // we need to increase this size even if the address is duplicate, because the birth time is different which
                // indicates two life periods of this addr
                internalMapSize++;
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
                outputAddressIndexSet.erase(addressIndex);
            }
            else if (inputAddressIndexSet.find(addressIndex) == inputAddressIndexSet.end())
            {
                firstStore(addressIndex, timing, false);
            }
            timing++;
        }
    }
} // namespace WorkingSet
