#include "WorkingSet.h"

using namespace std;

namespace WorkingSet
{
    int64_t timing = 0;
    map<int64_t, string> deAlias;
    map<string, vector<int64_t>> virAddr;
    uint64_t inputSize;
    void firstStore(string &addr, int64_t t, int op)
    {
        int64_t addrIndex = stol(addr, nullptr, 0);
        string VirAddrIndex;
        stringstream ss;
        if (op > 0)
        {
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

    void livingLoad(string &addr, int64_t t)
    {

        int64_t addrIndex = stol(addr, nullptr, 0);
        virAddr[deAlias[addrIndex]].push_back(t);
    }

    void Process(string &key, string &value)
    {
        int64_t addrIndex = stol(value, nullptr, 0);
        string &address = value;
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