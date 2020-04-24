#include "WorkingSet.h"

using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;
    uint64_t inputMapSize = 0;
    uint64_t internalMapSize = 0;

    //address living vector: to store the address structs of birth and death time
    vector<InputaddressLiving> inputAddressLivingVec;
    vector<InternaladdressLiving> internalAddressLivingVec;

    // key map to speed up the address seaching, because there are many same addresses in the address living vector, we need to locate the latest one.
    map<uint64_t, uint64_t> inputAddressIndexMap;
    map<uint64_t, uint64_t> internalAddressIndexMap;

    // FirstStore is called when we know that the address appears for the first time, then:
    // 1.Update key map with the live addresses
    // 2.Construct the address structs of birth and death time
    // Here “op” is a flag to indicate that if the address first appears from a load or a store
    void firstStore(uint64_t addrIndex, int64_t t, bool fromStore)
    {
        // birth from a store inst
        if (fromStore)
        {
            //structure 赋值 改改
            InternaladdressLiving internalAddress;
            internalAddress.address = addrIndex;
            internalAddress.brithTime = t;
            // death time is "-1" indicates only a store instruction with this address curently, no load instruction
            internalAddress.deathTime = -1;

            // update the death time for two continuous store instructions
            if (internalAddressIndexMap.find(addrIndex) != internalAddressIndexMap.end())
            {
                if (internalAddressLivingVec[internalAddressIndexMap[addrIndex]].deathTime == -1)
                {

                    internalAddressLivingVec[internalAddressIndexMap[addrIndex]].deathTime = t - 1;
                }
            }

            //update the map and the vector
            internalAddressIndexMap[addrIndex] = internalMapSize;
            internalAddressLivingVec.push_back(internalAddress);

            // we need to increase this size even if the address is duplicate, because the birth time is different which
            // indicates two life periods of this addr
            internalMapSize++;
        }
        // birth from a load inst
        else
        {
            inputAddressIndexMap[addrIndex] = inputMapSize;
            InputaddressLiving inputAddress;
            inputAddress.address = addrIndex;
            inputAddress.firstLoadTime = t;
            inputAddress.deathTime = t;
            inputAddressLivingVec.push_back(inputAddress);
            inputMapSize++;
        }
    }

    // LivingLoad:
    // Update the virtual address map of the live address:
    // The times when load instruction appears are pushed into virtual address map.

    void livingLoad(uint64_t addrIndex, int64_t t)
    {
        //the coming address is an internal address
        if (internalAddressIndexMap.find(addrIndex) != internalAddressIndexMap.end())
        {
            internalAddressLivingVec[internalAddressIndexMap[addrIndex]].deathTime = t;
        }

        if (inputAddressIndexMap.find(addrIndex) != inputAddressIndexMap.end())
        {
            inputAddressLivingVec[inputAddressIndexMap[addrIndex]].deathTime = t;
        }
        //an input address
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
            if (internalAddressIndexMap.find(addressIndex) != internalAddressIndexMap.end() || inputAddressIndexMap.find(addressIndex) != inputAddressIndexMap.end())
            {
                livingLoad(addressIndex, timing);
            }
            else
            {
                firstStore(addressIndex, timing, false);
            }
            timing++;
        }
    }
} // namespace WorkingSet