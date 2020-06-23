#include "WorkingSet.h"
#include "MemoryTool.h"
using namespace std;

namespace WorkingSet
{
    set<uint64_t> VBlock;
    set<uint64_t> VKernel;
    int64_t timing = 0;
    map<uint64_t,KernelWorkingSet> KernelWorkingSetMap;

    void firstStore(uint64_t addrIndex, int64_t t, bool fromStore, uint64_t kernelIndex)
    {
        // birth from a store inst
        if (fromStore)
        {
            //if(KernelWorkingSetMap[kernelIndex].internalAddressIndexMap.find(addrIndex)==KernelWorkingSetMap[kernelIndex].internalAddressIndexMap.end())
            {
                InternaladdressLiving internalAddress = {.birthTime = t, .deathTime = -1};
                //store the address into output set temporally
                KernelWorkingSetMap[kernelIndex].outputAddressIndexSet.insert(addrIndex);
                KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[addrIndex] = KernelWorkingSetMap[kernelIndex].internalMapSize;
                KernelWorkingSetMap[kernelIndex].internalAddressLivingVec.push_back(internalAddress);
                KernelWorkingSetMap[kernelIndex].internalMapSize++;
            }
            
        }
        // birth from a load inst
        else
        {
            KernelWorkingSetMap[kernelIndex].inputAddressIndexSet.insert(addrIndex);
            KernelWorkingSetMap[kernelIndex].inputMapSize++;
        }
    }
    void Process(string &key, string &value)
    {    
        
        if (key == "BBEnter")
        {
            if (kernelMap.find(stoul(value, nullptr, 0))!= kernelMap.end())
            {
                VKernel.clear();
                VBlock.insert(stoul(value, nullptr, 0));
                VKernel.insert(VBlock.begin(),VBlock.end());
            }
        }
        if (key == "BBExit")
        {
            if (kernelMap.find(stoul(value, nullptr, 0))!= kernelMap.end())
            {
                VKernel.clear();
                VBlock.erase(stoul(value, nullptr, 0));
                VKernel.insert(VBlock.begin(),VBlock.end());
            }
        }
        uint64_t addressIndex;
        for (auto it: VKernel)
        {
            if (key == "StoreAddress")
            {
                addressIndex = stoul(value, nullptr, 0);
                firstStore(addressIndex, timing, true, it);
                timing++;
            }
            else if (key == "LoadAddress")
            {
                addressIndex = stoul(value, nullptr, 0);
                //Update the death time in address struct if the address is already in internal address vector
                if (KernelWorkingSetMap[it].internalAddressIndexMap.find(addressIndex) != KernelWorkingSetMap[it].internalAddressIndexMap.end())
                {
                    KernelWorkingSetMap[it].internalAddressLivingVec[KernelWorkingSetMap[it].internalAddressIndexMap[addressIndex]].deathTime = timing;
                    //remove the address from output set, if there is a load corresponding to a store
                    KernelWorkingSetMap[it].outputAddressIndexSet.erase(addressIndex);
                }
                else if (KernelWorkingSetMap[it].inputAddressIndexSet.find(addressIndex) == KernelWorkingSetMap[it].inputAddressIndexSet.end())
                {
                    firstStore(addressIndex, timing, false, it);
                }
                timing++;
            }
        }  
    }

    
    void ProcessBlock(string &key, string &value)
    {   
        uint64_t addressIndex;
        if (key == "BBEnter")
        {
            if (ValidBlock.find(stoul(value, nullptr, 0)) != ValidBlock.end())
            {
                VBlock.insert(stoul(value, nullptr, 0));
            }
            //cout<< "BBID:"<<stoul(value, nullptr, 0)<<endl;
        }
        if (key == "BBExit")
        {
            if (ValidBlock.find(stoul(value, nullptr, 0)) != ValidBlock.end())
            {
                VBlock.erase(stoul(value, nullptr, 0));
            }
        }
        
        //for (auto it: VBlock)
        {
            if (key == "StoreAddress")
            {
                addressIndex = stoul(value, nullptr, 0);
                //firstStore(addressIndex, timing, true, it);
                timing++;
            }
            else if (key == "LoadAddress")
            {
                // addressIndex = stoul(value, nullptr, 0);
                // //Update the death time in address struct if the address is already in internal address vector
                // if (KernelWorkingSetMap[it].internalAddressIndexMap.find(addressIndex) != KernelWorkingSetMap[it].internalAddressIndexMap.end())
                // {
                //     KernelWorkingSetMap[it].internalAddressLivingVec[KernelWorkingSetMap[it].internalAddressIndexMap[addressIndex]].deathTime = timing;
                //     //remove the address from output set, if there is a load corresponding to a store
                //     KernelWorkingSetMap[it].outputAddressIndexSet.erase(addressIndex);
                // }
                // else if (KernelWorkingSetMap[it].inputAddressIndexSet.find(addressIndex) == KernelWorkingSetMap[it].inputAddressIndexSet.end())
                // {
                //     firstStore(addressIndex, timing, false, it);
                // }
                // timing++;
            }
        }
    }
} // namespace WorkingSet