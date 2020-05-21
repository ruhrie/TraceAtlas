#include "WorkingSet.h"

using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;
    uint64_t inputMapSize = 0;
    uint64_t internalMapSize = 0;

    //internal address struct vector: to store the address structs of birth and death time
    vector<InternaladdressLiving> internalAddressLivingVec;
    //internal address index map: to speed up the addresses seaching in the internal address struct vector
    map<uint64_t, uint64_t> internalAddressIndexMap;
    //input address index set: to store the input address indexes
    set<uint64_t> inputAddressIndexSet;
    //output address index set: to store the output address indexes
    set<uint64_t> outputAddressIndexSet;
    // FirstStore is called when we know that the address appears for the first time, then:
    // 1.Update address index map
    // 2.Construct the address structs of birth and death time
    // “fromStore” is a flag to indicate that if the address first appears from a load or a store instruction
    void firstStore(uint64_t addrIndex, int64_t t, bool fromStore)
    {
        // birth from a store inst
        if (fromStore)
        {
            InternaladdressLiving internalAddress = {.address = addrIndex, .birthTime = t, .deathTime = -1};
            //store the address into output set temporally
            outputAddressIndexSet.insert(addrIndex);
            //update the map and the vector, if the address has never shown before
            internalAddressIndexMap[addrIndex] = internalMapSize;
            internalAddressLivingVec.push_back(internalAddress);
            internalMapSize++;
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
            //Update the death time in address struct if the address is already in internal address vector
            if (internalAddressIndexMap.find(addressIndex) != internalAddressIndexMap.end())
            {
                internalAddressLivingVec[internalAddressIndexMap[addressIndex]].deathTime = timing;
                //remove the address from output set, if there is a load corresponding to a store
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