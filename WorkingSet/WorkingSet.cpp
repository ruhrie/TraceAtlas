#include "inc/WorkingSet.h"
#include <algorithm>
#include <indicators/progress_bar.hpp>
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
    map<uint64_t, pair<uint64_t, uint64_t>> addrLifeSpanMap;
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
            addrLifeSpanMap[addr].second = timeCount;
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
            if (addrLifeSpanMap.find(addr) == addrLifeSpanMap.end())
            {
                addrLifeSpanMap[addr].first = timeCount;
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
    map<uint64_t, uint64_t, greater<>> BirthTimeMap;
    map<uint64_t, uint64_t, greater<>> DeathTimeMap;
    /// Maps a kernel ID to the max live address counts of each working set and total
    map<int, vector<uint64_t>> kernelWSLiveAddrMaxCounts;
    void DynamicSetSizes(bool nobar)
    {
        // indicators for time parsing progress
        std::cout << "\e[?25l";
        indicators::ProgressBar bar;
        int previousCount = 0;
        if (!nobar)
        {
            bar.set_option(indicators::option::PrefixText{"Parsing dynamic working sets."});
            bar.set_option(indicators::option::ShowElapsedTime{true});
            bar.set_option(indicators::option::ShowRemainingTime{true});
            bar.set_option(indicators::option::BarWidth{50});
        }
        // reverse map addrLifeSpanMap using BirthTimeMap and DeathTimeMap
        for (const auto &addr : addrLifeSpanMap)
        {
            DeathTimeMap[addr.second.second] = addr.first;
            BirthTimeMap[addr.second.first] = addr.first;
        }

        // Find the max time point of our trace
        uint64_t maxTime = max(BirthTimeMap.begin()->first, DeathTimeMap.begin()->first);
        if (maxTime == 0)
        {
            spdlog::critical("The maxtime as parsed from the trace is 0. Exiting...");
            return;
        }
        uint64_t timeDivide = maxTime / 10000;
        // initialize our max count map, initialize our live address set map
        map<int, vector<set<uint64_t>>> liveAddressSetMap;
        for (const auto &kernelID : kernelSetMap)
        {
            // max count map
            kernelWSLiveAddrMaxCounts[kernelID.first] = vector<uint64_t>(4);
            kernelWSLiveAddrMaxCounts[kernelID.first][0] = 0;
            kernelWSLiveAddrMaxCounts[kernelID.first][1] = 0;
            kernelWSLiveAddrMaxCounts[kernelID.first][2] = 0;
            kernelWSLiveAddrMaxCounts[kernelID.first][3] = 0;
            // live address set map
            liveAddressSetMap[kernelID.first] = vector<set<uint64_t>>(4);
            liveAddressSetMap[kernelID.first][0] = set<uint64_t>();
            liveAddressSetMap[kernelID.first][1] = set<uint64_t>();
            liveAddressSetMap[kernelID.first][2] = set<uint64_t>();
            liveAddressSetMap[kernelID.first][3] = set<uint64_t>();
        }
        vector<int> currentKernels = vector<int>();
        // go through the entire trace time
        for (uint64_t timeCount = 0; timeCount < maxTime + 1; timeCount++)
        {
            // find the map that has our timestamp in it
            if (BirthTimeMap.find(timeCount) != BirthTimeMap.end())
            {
                // find the kernel IDs that have this address
                for (const auto &key : kernelSetMap)
                {
                    if ((key.second.first.find(BirthTimeMap[timeCount]) != key.second.first.end()) || (key.second.second.find(BirthTimeMap[timeCount]) != key.second.second.end()))
                    {
                        currentKernels.push_back(key.first);
                    }
                }
                // for each kernel our address belongs to, add it to the total live address set
                for (const auto &ind : currentKernels)
                {
                    liveAddressSetMap[ind][3].insert(BirthTimeMap[timeCount]);
                    // if this address belongs to the input working set
                    if (kernelWSMap[ind][0].find(BirthTimeMap[timeCount]) != kernelWSMap[ind][0].end())
                    {
                        liveAddressSetMap[ind][0].insert(BirthTimeMap[timeCount]);
                    }
                    // if this address belongs to the output working set
                    if (kernelWSMap[ind][2].find(BirthTimeMap[timeCount]) != kernelWSMap[ind][2].end())
                    {
                        liveAddressSetMap[ind][2].insert(BirthTimeMap[timeCount]);
                    }
                    // if this address belongs to the internal working set
                    if (kernelWSMap[ind][1].find(BirthTimeMap[timeCount]) != kernelWSMap[ind][1].end())
                    {
                        liveAddressSetMap[ind][1].insert(BirthTimeMap[timeCount]);
                    }
                    // update our max counts
                    if (kernelWSLiveAddrMaxCounts[ind][0] < liveAddressSetMap[ind][0].size())
                    {
                        kernelWSLiveAddrMaxCounts[ind][0] = liveAddressSetMap[ind][0].size();
                    }
                    if (kernelWSLiveAddrMaxCounts[ind][1] < liveAddressSetMap[ind][1].size())
                    {
                        kernelWSLiveAddrMaxCounts[ind][1] = liveAddressSetMap[ind][1].size();
                    }
                    if (kernelWSLiveAddrMaxCounts[ind][2] < liveAddressSetMap[ind][2].size())
                    {
                        kernelWSLiveAddrMaxCounts[ind][2] = liveAddressSetMap[ind][2].size();
                    }
                    if (kernelWSLiveAddrMaxCounts[ind][3] < liveAddressSetMap[ind][3].size())
                    {
                        kernelWSLiveAddrMaxCounts[ind][3] = liveAddressSetMap[ind][3].size();
                    }
                }
            }
            else if (DeathTimeMap.find(timeCount) != DeathTimeMap.end())
            {
                // find the kernel IDs that have this address
                for (const auto &key : kernelSetMap)
                {
                    if ((key.second.first.find(DeathTimeMap[timeCount]) != key.second.first.end()) || (key.second.second.find(DeathTimeMap[timeCount]) != key.second.second.end()))
                    {
                        currentKernels.push_back(key.first);
                    }
                }
                for (const auto &ind : currentKernels)
                {
                    liveAddressSetMap[ind][3].erase(DeathTimeMap[timeCount]);
                    // if this address belongs to the input working set
                    if (kernelWSMap[ind][0].find(DeathTimeMap[timeCount]) != kernelWSMap[ind][0].end())
                    {
                        liveAddressSetMap[ind][0].erase(DeathTimeMap[timeCount]);
                    }
                    // if this address belongs to the output working set
                    if (kernelWSMap[ind][2].find(DeathTimeMap[timeCount]) != kernelWSMap[ind][2].end())
                    {
                        liveAddressSetMap[ind][2].erase(DeathTimeMap[timeCount]);
                    }
                    // if this address belongs to the internal working set
                    if (kernelWSMap[ind][1].find(DeathTimeMap[timeCount]) != kernelWSMap[ind][1].end())
                    {
                        liveAddressSetMap[ind][1].erase(DeathTimeMap[timeCount]);
                    }
                }
            }
            currentKernels.clear();
            if (!nobar)
            {
                if (timeCount % timeDivide == 0)
                {
                    double percent = (double)timeCount / (double)maxTime * (double)100.0;
                    bar.set_progress(percent);
                    bar.set_option(indicators::option::PostfixText{"Time " + std::to_string(timeCount) + "/" + std::to_string(maxTime)});
                }
            }
        }
        if (!nobar && !bar.is_completed())
        {
            bar.mark_as_completed();
        }
        std::cout << "\e[?25h";
    }
} // namespace WorkingSet