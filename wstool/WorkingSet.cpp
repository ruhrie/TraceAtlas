#include "WorkingSet.h"
#include "MemoryTool.h"
using namespace std;

namespace WorkingSet
{
    set<uint64_t> VBlock;
    set<uint64_t> VKernel;
    
    int64_t timing = 0;
    map<uint64_t,KernelWorkingSet> KernelWorkingSetMap;
    vector<InternaladdressLiving> internalAddressLivingVec;
    map<uint64_t,uint64_t> kernelFiringNum;
    map<uint64_t,uint64_t> maxinternalfiring;
    map<uint64_t,vector<uint64_t>> internalTimeStamp;
    void firstStore(uint64_t addrIndex, int64_t t, bool fromStore, uint64_t kernelIndex)
    {
        // birth from a store inst
        if (fromStore)
        {
            if(KernelWorkingSetMap[kernelIndex].internalAddressIndexMap.find(addrIndex)==KernelWorkingSetMap[kernelIndex].internalAddressIndexMap.end())
            {
                InternaladdressLiving internalAddress = {.addr =addrIndex, .birthTime = t, .deathTime = -1,.dep =false};
                //store the address into output set temporally
                KernelWorkingSetMap[kernelIndex].outputAddressIndexSet.insert(addrIndex);
                KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[addrIndex] = KernelWorkingSetMap[kernelIndex].internalMapSize;
                KernelWorkingSetMap[kernelIndex].internalAddressLivingVec.push_back(internalAddress);
                KernelWorkingSetMap[kernelIndex].internalMapSize++;
            }
            else
            {
                InternaladdressLiving internalAddress = {.addr =addrIndex, .birthTime = t, .deathTime = -1,.dep =false};
                //store the address into output set temporally
                KernelWorkingSetMap[kernelIndex].outputAddressIndexSet.insert(addrIndex);
                KernelWorkingSetMap[kernelIndex].internalAddressLivingVec[KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[addrIndex]].dep = true;
                KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[addrIndex] = KernelWorkingSetMap[kernelIndex].internalMapSize;
                KernelWorkingSetMap[kernelIndex].internalAddressLivingVec.push_back(internalAddress);
                KernelWorkingSetMap[kernelIndex].internalMapSize++;
                kernelFiringNum[kernelIndex] = KernelWorkingSetMap[kernelIndex].internalMapSize - KernelWorkingSetMap[kernelIndex].internalAddressIndexMap.size();
            }
            
        }
        //birth from a load inst
        else
        {
            KernelWorkingSetMap[kernelIndex].inputAddressIndexSet.insert(addrIndex);
            KernelWorkingSetMap[kernelIndex].inputMapSize++;
        }
    }


    void dynamicSize (uint64_t kernelIndex)
    {
        set<int64_t> endTimeSet;
        for (auto it : KernelWorkingSetMap[kernelIndex].internalAddressLivingVec)
        {
            if (it.deathTime > 0)
            {
                endTimeSet.insert(it.deathTime);
                while (it.birthTime > *(endTimeSet.begin()))
                {
                    endTimeSet.erase(endTimeSet.begin());
                }
            }
        }
        internalTimeStamp[kernelIndex].push_back(endTimeSet.size());
    }




    void firingClear (uint64_t kernelIndex)
    {
        set<int64_t> endTimeSet;
        for (auto it : KernelWorkingSetMap[kernelIndex].internalAddressLivingVec)
        {
            if (it.deathTime > 0)
            {
                endTimeSet.insert(it.deathTime);
                while (it.birthTime > *(endTimeSet.begin()))
                {
                    endTimeSet.erase(endTimeSet.begin());
                }
                if (endTimeSet.size() > maxinternalfiring[kernelIndex])
                {
                    maxinternalfiring[kernelIndex] = endTimeSet.size();
                }
            }
        }
        uint64_t counterF = 0;
        vector<InternaladdressLiving> temp;
        for (auto it :KernelWorkingSetMap[kernelIndex].internalAddressLivingVec)
        {
            if (it.dep == false)
            {
                temp.push_back(it);
                KernelWorkingSetMap[kernelIndex].internalAddressIndexMap[it.addr]= counterF;
                counterF++;
            }
        }
        KernelWorkingSetMap[kernelIndex].internalAddressLivingVec.clear();
        KernelWorkingSetMap[kernelIndex].internalAddressLivingVec = temp;
        KernelWorkingSetMap[kernelIndex].internalMapSize = KernelWorkingSetMap[kernelIndex].internalAddressIndexMap.size();
    }

    void Process(string &key, string &value)
    {        
        if (key == "BBEnter")
        {
            if (kernelMap.find(stoul(value, nullptr, 0))!= kernelMap.end())
            {
                
                VKernel.clear();
                VBlock.insert(stoul(value, nullptr, 0));
                for (auto it : VBlock)
                {
                    VKernel.insert(kernelMap[it].begin(),kernelMap[it].end());
                }           
            }
        }
        if (key == "BBExit")
        {
            if (kernelMap.find(stoul(value, nullptr, 0))!= kernelMap.end())
            {
                
                VKernel.clear();
                VBlock.erase(stoul(value, nullptr, 0));
                for (auto it : VBlock)
                {
                    VKernel.insert(kernelMap[it].begin(),kernelMap[it].end());
                }           
            }
        }
        uint64_t addressIndex;
        if ((key == "StoreAddress")||(key == "LoadAddress"))
        {
            for (auto it: VKernel)
            {
                if (key == "StoreAddress")
                {
                    addressIndex = stoul(value, nullptr, 0);
                    firstStore(addressIndex, timing, true, it);
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
                }
                if(kernelFiringNum[it]>30000)
                {
                    //firingClear(it);
                }
            }
            timing++;
        }  
    }

 } // namespace WorkingSet