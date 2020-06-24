#include "inc/WorkingSet.h"
#include <algorithm>
#include <iostream>
#include <spdlog/spdlog.h>

using namespace std;
namespace WorkingSet
{
    /// Maps a kernel index to its set of basic block IDs
    map<int, set<int64_t>> kernelBlockMap;
    void Setup(nlohmann::json &j)
    {
        for (auto &[k, l] : j["Kernels"].items())
        {
            int index = stoi(k);
            nlohmann::json kernel = l["Blocks"];
            kernelBlockMap[index] = kernel.get<set<int64_t>>();
        }
    }

    /// Global time keeper
    uint64_t timeCount = 0;
    /// Maps an address to a pair that holds the time of its first store and last load
    /// first -> birth time, second -> death time
    map<uint64_t, pair<uint64_t, uint64_t>> addrLiveSpanMap;
    /// Maps a kernel index to a pair of sets (first -> ld addr, second -> st addr)
    map<int, pair<set<uint64_t>, set<uint64_t>>> kernelSetMap;
    vector<int> currentKernelIDs;
    void Process(string &key, string &value)
    {
        if (key == "BBEnter")
        {
            int64_t blockID = stoi(value, nullptr, 0);
            for (const auto &ID : kernelBlockMap)
            {
                if (ID.second.find(blockID) != ID.second.end())
                {
                    currentKernelIDs.push_back(ID.first);
                }
            }
        }
        else if (key == "LoadAddress")
        {
            uint64_t addr = stoul(value, nullptr, 0);
            // death time, constantly updating
            addrLiveSpanMap[addr].second = timeCount;
            for (const auto &ind : currentKernelIDs)
            {
                kernelSetMap[ind].first.insert(addr);
            }
            timeCount++;
        }
        else if (key == "StoreAddress")
        {
            uint64_t addr = stoul(value, nullptr, 0);
            // birth time
            if (addrLiveSpanMap.find(addr) == addrLiveSpanMap.end())
            {
                addrLiveSpanMap[addr].first = timeCount;
            }
            for (const auto &ind : currentKernelIDs)
            {
                kernelSetMap[ind].second.insert(addr);
            }
            timeCount++;
        }
        else if (key == "BBExit")
        {
            currentKernelIDs.clear();
        }
    }

    /// Maps a kernel index to its working sets
    /// 0-> input, 1->internal, 2->output
    map<int, vector<set<uint64_t>>> kernelWSMap;
    void CreateSets()
    {
        for (const auto &key : kernelSetMap)
        {
            /// Allocate three positions for each of the sets
            kernelWSMap[key.first] = vector<set<uint64_t>>(3);
            /// Intersect the ld and st sets
            vector<uint64_t> intersect;
            if (key.second.first.size() > key.second.second.size())
            {
                intersect = vector<uint64_t>(key.second.first.size());
            }
            else
            {
                intersect = vector<uint64_t>(key.second.first.size());
            }
            auto it = set_intersection(key.second.first.begin(), key.second.first.end(), key.second.second.begin(), key.second.second.end(), intersect.begin());
            intersect.resize(it - intersect.begin());
            kernelWSMap[key.first][1] = set<uint64_t>(intersect.begin(), intersect.end());

            /// input working set = ld set - intersect
            kernelWSMap[key.first][0] = set<uint64_t>();
            for (const auto &ind : kernelSetMap[key.first].first)
            {
                if (kernelWSMap[key.first][1].find(ind) == kernelWSMap[key.first][1].end())
                {
                    kernelWSMap[key.first][0].insert(ind);
                }
            }

            // output working set = st set - intersect
            kernelWSMap[key.first][2] = set<uint64_t>();
            for (const auto &ind : kernelSetMap[key.first].second)
            {
                if (kernelWSMap[key.first][1].find(ind) == kernelWSMap[key.first][1].end())
                {
                    kernelWSMap[key.first][2].insert(ind);
                }
            }
        }
    }

    /// Maps kernel ID pairs to a vector of sets (length 3) containing the intersections of each kernel's working sets
    map<pair<int, int>, vector<set<uint64_t>>> ProdConMap;
    void StaticSetSizes()
    {
        for (auto it0 = kernelWSMap.begin(); it0 != kernelWSMap.end(); it0++)
        {
            /// for each kernel, intersect with all other kernels
            for (auto it1 = it0; it1 != kernelWSMap.end(); it1++)
            {
                if (it0->first == it1->first)
                {
                    continue;
                }

                /// Create vector of set intersections
                /// 0: it0->input working set    & it1->output working set
                /// 1: it0->internal working set & it1->internal working set
                /// 2: it0->output working set   & it1->input working set
                pair<int, int> newPair = pair(it0->first, it1->first);
                ProdConMap[newPair] = vector<set<uint64_t>>(3);

                /// it0->input working set & it1->output working set
                vector<uint64_t> intersect;
                if (it0->second[0].size() > it1->second[2].size())
                {
                    intersect = vector<uint64_t>(it0->second[0].size());
                }
                else
                {
                    intersect = vector<uint64_t>(it1->second[2].size());
                }
                auto it = set_intersection(it0->second[0].begin(), it0->second[0].end(), it1->second[2].begin(), it1->second[2].end(), intersect.begin());
                intersect.resize(it - intersect.begin());
                ProdConMap[newPair][0] = set<uint64_t>(intersect.begin(), intersect.end());

                /// it0->internal working set & it1->internal working set
                intersect.clear();
                if (it0->second[1].size() > it1->second[1].size())
                {
                    intersect = vector<uint64_t>(it0->second[1].size());
                }
                else
                {
                    intersect = vector<uint64_t>(it1->second[1].size());
                }
                it = set_intersection(it0->second[1].begin(), it0->second[1].end(), it1->second[1].begin(), it1->second[1].end(), intersect.begin());
                intersect.resize(it - intersect.begin());
                ProdConMap[newPair][1] = set<uint64_t>(intersect.begin(), intersect.end());

                /// it0->output working set & it1->input working set
                intersect.clear();
                if (it0->second[2].size() > it1->second[0].size())
                {
                    intersect = vector<uint64_t>(it0->second[2].size());
                }
                else
                {
                    intersect = vector<uint64_t>(it1->second[0].size());
                }
                it = set_intersection(it0->second[2].begin(), it0->second[2].end(), it1->second[0].begin(), it1->second[0].end(), intersect.begin());
                intersect.resize(it - intersect.begin());
                ProdConMap[newPair][2] = set<uint64_t>(intersect.begin(), intersect.end());
            }
        }
    }

    /// Maps a time in the trace to the birth or death time of an address
    map<int, map<uint64_t, uint64_t, greater<>>> BirthTimeMap;
    map<int, map<uint64_t, uint64_t, greater<>>> DeathTimeMap;
    /// Maps a kernel ID to its max alive address count
    map<int, unsigned long> liveAddressMaxCounts;
    /// Maps a kernel ID to the max live address counts of each working set
    map<int, vector<uint64_t>> kernelWSLiveAddrMaxCounts;
    void DynamicSetSizes()
    {
        // reverse map addrLiveSpanMap using BirthTimeMap and DeathTimeMap
        for (const auto &addr : addrLiveSpanMap)
        {
            // find kernel membership
            vector<int> kernelIDs = vector<int>();
            for (const auto &key : kernelSetMap)
            {
                if (key.second.first.find(addr.first) != key.second.first.end())
                {
                    kernelIDs.push_back(key.first);
                }
                else if (key.second.second.find(addr.first) != key.second.second.end())
                {
                    kernelIDs.push_back(key.first);
                }
            }
            if (kernelIDs.empty())
            {
                continue;
            }
            for (const auto &ID : kernelIDs)
            {
                DeathTimeMap[ID][addr.second.second] = addr.first;
                BirthTimeMap[ID][addr.second.first] = addr.first;
            }
        }
        // now go key by key in the birth and death time maps and keep a count of alive addresses
        set<uint64_t> liveAddresses;
        for (const auto &kernelID : BirthTimeMap)
        {
            spdlog::info("Creating total live address counts for kernel index " + to_string(kernelID.first));
            liveAddressMaxCounts[kernelID.first] = 0;
            liveAddresses.clear();
            uint64_t minTime = min(prev(BirthTimeMap[kernelID.first].end())->first, prev(DeathTimeMap[kernelID.first].end())->first);
            uint64_t maxTime = max(BirthTimeMap[kernelID.first].begin()->first, DeathTimeMap[kernelID.first].begin()->first);
            for (uint64_t timeCount = minTime - 1; timeCount < maxTime + 1; timeCount++)
            {
                if (BirthTimeMap[kernelID.first].find(timeCount) != BirthTimeMap[kernelID.first].end())
                {
                    liveAddresses.insert(BirthTimeMap[kernelID.first][timeCount]);
                }
                if (DeathTimeMap[kernelID.first].find(timeCount) != DeathTimeMap[kernelID.first].end())
                {
                    liveAddresses.erase(DeathTimeMap[kernelID.first][timeCount]);
                }
                if (liveAddressMaxCounts[kernelID.first] < liveAddresses.size())
                {
                    liveAddressMaxCounts[kernelID.first] = liveAddresses.size();
                }
            }
        }

        // project the same idea onto the individual sets of each kernel index
        // vector to hold our separate live address sets, indexed in the same way as kernelWSLiveAddrMaxCounts
        auto liveAddrSets = vector<set<uint64_t>>(3);
        for (const auto &kernelID : BirthTimeMap)
        {
            spdlog::info("Creating live address counts on each working set for kernel index " + to_string(kernelID.first));
            kernelWSLiveAddrMaxCounts[kernelID.first] = vector<uint64_t>(3);
            for (auto &ind : liveAddrSets)
            {
                ind.clear();
            }
            uint64_t minTime = max(prev(BirthTimeMap[kernelID.first].end())->first, prev(DeathTimeMap[kernelID.first].end())->first);
            uint64_t maxTime = max(BirthTimeMap[kernelID.first].begin()->first, DeathTimeMap[kernelID.first].begin()->first);
            for (uint64_t timeCount = minTime - 1; timeCount < maxTime + 1; timeCount++)
            {
                // if this is a birth address
                if (BirthTimeMap[kernelID.first].find(timeCount) != BirthTimeMap[kernelID.first].end())
                {
                    // if we are an input set addr
                    if (kernelWSMap[kernelID.first][0].find(BirthTimeMap[kernelID.first][timeCount]) != kernelWSMap[kernelID.first][0].end())
                    {
                        liveAddrSets[0].insert(BirthTimeMap[kernelID.first][timeCount]);
                    }
                    // if we are an output set addr
                    else if (kernelWSMap[kernelID.first][1].find(BirthTimeMap[kernelID.first][timeCount]) != kernelWSMap[kernelID.first][1].end())
                    {
                        liveAddrSets[1].insert(BirthTimeMap[kernelID.first][timeCount]);
                    }
                    // if we are an output set addr
                    else if (kernelWSMap[kernelID.first][2].find(BirthTimeMap[kernelID.first][timeCount]) != kernelWSMap[kernelID.first][2].end())
                    {
                        liveAddrSets[2].insert(BirthTimeMap[kernelID.first][timeCount]);
                    }
                }
                // if this is a death address
                else if (DeathTimeMap[kernelID.first].find(timeCount) != DeathTimeMap[kernelID.first].end())
                {
                    // if we are an input set addr
                    if (kernelWSMap[kernelID.first][0].find(BirthTimeMap[kernelID.first][timeCount]) != kernelWSMap[kernelID.first][0].end())
                    {
                        liveAddrSets[0].erase(DeathTimeMap[kernelID.first][timeCount]);
                    }
                    // if we are an internal set addr
                    else if (kernelWSMap[kernelID.first][1].find(BirthTimeMap[kernelID.first][timeCount]) != kernelWSMap[kernelID.first][1].end())
                    {
                        liveAddrSets[1].erase(DeathTimeMap[kernelID.first][timeCount]);
                    }
                    // if we are an output set addr
                    else if (kernelWSMap[kernelID.first][2].find(BirthTimeMap[kernelID.first][timeCount]) != kernelWSMap[kernelID.first][2].end())
                    {
                        liveAddrSets[2].erase(DeathTimeMap[kernelID.first][timeCount]);
                    }
                }
                // update our sizes
                if (kernelWSLiveAddrMaxCounts[kernelID.first][0] < liveAddrSets[0].size())
                {
                    kernelWSLiveAddrMaxCounts[kernelID.first][0] = liveAddrSets[0].size();
                }
                if (kernelWSLiveAddrMaxCounts[kernelID.first][1] < liveAddrSets[1].size())
                {
                    kernelWSLiveAddrMaxCounts[kernelID.first][1] = liveAddrSets[1].size();
                }
                if (kernelWSLiveAddrMaxCounts[kernelID.first][2] < liveAddrSets[2].size())
                {
                    kernelWSLiveAddrMaxCounts[kernelID.first][2] = liveAddrSets[2].size();
                }
            }
        }
    }
} // namespace WorkingSet