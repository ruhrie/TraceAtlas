#include "WorkingSet.h"

using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;
    int64_t inputMapSize = 0;
    int64_t internalMapSize = 0;
    // DeAlias map: map to the last updated addresses:
    // DeAlias[addr] = key of Virtual address map
    map<int64_t, int64_t> deAliasInternal;
    map<int64_t, int64_t> deAliasInput;
    // Virtual address map: contains the time on the liveness of an address
    // Input and internal are seperated here, because it will be good for map erase and early termination while calculating ws size 
    map<int64_t, vector<int64_t>> internalVirAddr;
    map<int64_t, vector<int64_t>> inputVirAddr;

    // FirstStore is called when we know that the address appears for the first time, then:
    // 1.Update Alias table with the live addresses
    // 2.Construct the virtual address dictionary of the live addresses
    // Here “op” is a flag to indicate that if the address first appears from a load or a store
    void firstStore(string &addr, int64_t t, int op)
    {
        int64_t addrIndex = stol(addr, nullptr, 0);
        // birth from a store inst
        if (op > 0)
        {            
            // virAddr[order from 0 to total address number] = [addr, t0, t1, t2,…,tn]
            // t0: the time when the first store appears, it’s the time the address begins
            // t1, t2, …, tn: the time when the load instruction appears to load from this address
            vector<int64_t> virAddrLine(2);
            virAddrLine[0] = addrIndex;
            virAddrLine[1] = t;

            internalVirAddr[internalMapSize] = virAddrLine;
            //the coming address is not in the map
            if (deAliasInternal.count(addrIndex) == 0)
            {
                deAliasInternal[addrIndex] = internalMapSize;
            } //the coming address in the map is not bigger than 3, indicates only store instruction
            //has came but no load instructions
            else if (internalVirAddr[deAliasInternal[addrIndex]].size() < 3)
            {
                //manually adding an end time for this address
                internalVirAddr[deAliasInternal[addrIndex]].push_back(t - 1);
                deAliasInternal[addrIndex] = internalMapSize;
            }
            else
            {
                deAliasInternal[addrIndex] = internalMapSize;
            }
            // we need to increase this size even if the address is duplicate, because the birth time is different which
            // indicates two life periods of this addr
            internalMapSize++; 
        }
        // birth from a load inst
        else
        {
            //the coming address is not in the map
            if (deAliasInput.count(addrIndex) == 0)
            {
                vector<int64_t> virAddrLine(3);
                virAddrLine[0] = addrIndex;
                //the birth time is "-1"
                virAddrLine[1] = -1;
                virAddrLine[2] = t;

                inputVirAddr[inputMapSize] = virAddrLine;
                deAliasInput[addrIndex] = inputMapSize;
                inputMapSize++;
            }
            else
            {
                inputVirAddr[deAliasInput[addrIndex]].push_back(t);
            }
        }
    }

    // LivingLoad:
    // Update the virtual address map of the live address:
    // The times when load instruction appears are pushed into virtual address map.

    void livingLoad(string &addr, int64_t t)
    {

        int64_t addrIndex = stol(addr, nullptr, 0);
        //the coming address is an internal address
        if (deAliasInternal.count(addrIndex) != 0)
        {
            internalVirAddr[deAliasInternal[addrIndex]].push_back(t);
        }
        //an input address
        else
        {
            inputVirAddr[deAliasInput[addrIndex]].push_back(t);
        }
    }

    void Process(string &key, string &value)
    {
        int64_t addrIndex;
        string &address = value;
        if ((key == "StoreAddress") || (key == "LoadAddress"))
        {
            addrIndex = stol(value, nullptr, 0);
        }
        // a load instruction with addresses already in our two virtual address maps.
        if ((deAliasInternal.count(addrIndex) != 0 || deAliasInput.count(addrIndex) != 0) && (key == "LoadAddress"))
        {
            livingLoad(address, timing);
            timing++;
        }
        // a load instruction with addresses not in our virtual address map.
        else if (key == "LoadAddress")
        {
            firstStore(address, timing, -1);
            timing++;
        }
        // a store instruction with addresses not in our virtual address map.
        else if (key == "StoreAddress")
        {
            firstStore(address, timing, 1);
            timing++;
        }
    }
} // namespace WorkingSet