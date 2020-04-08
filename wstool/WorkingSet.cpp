#include "WorkingSet.h"

using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;

    // DeAlias map: map to the last updated addresses:
    // DeAlias[addr] = key of Virtual address map
    map<int64_t, string> deAlias;
    map<string, vector<int64_t>> virAddr;
    uint64_t inputSize;

    // FirstStore is called when we know that the address appears for the first time, then:
    // 1.Update Alias table with the live addresses
    // 2.Construct the virtual address dictionary of the live addresses

    // Here “op” is a flag to indicate that the address is known to birth
    // because of a load instruction or Store instruction

    void firstStore(string &addr, int64_t t, int op)
    {
        int64_t addrIndex = stol(addr, nullptr, 0);
        string VirAddrIndex;
        stringstream ss;
        if (op > 0)
        {
            // Virtual address map: contains the time on the liveness of an address
            // virAddr[address + '@' + t0] = [addr, t0, t1, t2,…,tn]
            // t0: the time when the first store appears, it’s the time the address begins
            // t1, t2, …, tn: the time when the load instruction appears to load from this address
            vector<int64_t> virAddrLine(2);
            virAddrLine[0] = addrIndex;
            virAddrLine[1] = t;

            ss << addr << "@" << t;
            VirAddrIndex.append(ss.str());

            virAddr[VirAddrIndex] = virAddrLine;

            if (deAlias.count(addrIndex) == 0)
            {
                deAlias[addrIndex] = VirAddrIndex;
            }
            else if (virAddr[deAlias[addrIndex]].size() < 3)
            {
                virAddr[deAlias[addrIndex]].push_back(t - 1);
                deAlias[addrIndex] = VirAddrIndex;
            }
            else
            {
                deAlias[addrIndex] = VirAddrIndex;
            }
        }
        else
        {
            if (deAlias.count(addrIndex) == 0)
            {
                vector<int64_t> virAddrLine(3);
                virAddrLine[0] = addrIndex;
                virAddrLine[1] = -1;
                virAddrLine[2] = t;

                ss << addr << "@-1";
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

    // LivingLoad:
    // Update the virtual address map of the live address:
    // The times when load instruction appears are pushed into virtual address map.

    void livingLoad(string &addr, int64_t t)
    {

        int64_t addrIndex = stol(addr, nullptr, 0);
        virAddr[deAlias[addrIndex]].push_back(t);
    }

    void Process(string &key, string &value)
    {
        int64_t addrIndex;
        string &address = value;
        if ((key == "StoreAddress") || (key == "LoadAddress"))
        {
            addrIndex = stol(value, nullptr, 0);
        }
        if ((deAlias.count(addrIndex) != 0) && (key == "LoadAddress"))
        {
            livingLoad(address, timing);
            timing++;
        }
        else if (key == "LoadAddress")
        {
            firstStore(address, timing, -1);
            timing++;
        }
        else if (key == "StoreAddress")
        {
            firstStore(address, timing, 1);
            timing++;
        }
    }
} // namespace WorkingSet