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
    /// Maps a kernel index to AddressSets
    map<int, AddressSets> AddressSetMap;
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
                AddressSetMap[ind].load.insert(addr);
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
                AddressSetMap[ind].store.insert(addr);
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
    map<int, struct StaticSets> StaticWSMap;
    void CreateStaticSets()
    {
        for (const auto &key : AddressSetMap)
        {
            /// Allocate three positions for each of the sets
            StaticWSMap[key.first] = StaticSets();
            /// Intersect the ld and st sets
            vector<uint64_t> intersect;
            if (key.second.load.size() > key.second.store.size())
            {
                intersect = vector<uint64_t>(key.second.load.size());
            }
            else
            {
                intersect = vector<uint64_t>(key.second.store.size());
            }
            // intersect the load and store sets to generate internal working set
            auto it = set_intersection(key.second.load.begin(), key.second.load.end(), key.second.store.begin(), key.second.store.end(), intersect.begin());
            intersect.resize(it - intersect.begin());
            StaticWSMap[key.first].internal.insert(intersect.begin(), intersect.end());

            /// input working set = ld set - intersect
            // each address not in the internal set is in the input set
            for (const auto &ind : AddressSetMap[key.first].load)
            {
                if (StaticWSMap[key.first].internal.find(ind) == StaticWSMap[key.first].internal.end())
                {
                    StaticWSMap[key.first].input.insert(ind);
                }
            }

            // output working set = st set - intersect
            // each store address not in the internal set is in the output set
            for (const auto &ind : AddressSetMap[key.first].store)
            {
                if (StaticWSMap[key.first].internal.find(ind) == StaticWSMap[key.first].internal.end())
                {
                    StaticWSMap[key.first].output.insert(ind);
                }
            }
        }
    }

    /// Maps kernel ID pairs to a ProdCon struct
    vector<struct ProdCon> ProdConRelationships;
    // The set of relationships amounts to combinations of length 2 without replacement
    void ProducerConsumer()
    {
        // for each kernel index
        for (auto it0 = StaticWSMap.begin(); it0 != StaticWSMap.end(); it0++)
        {
            /// for each kernel, intersect with all other kernels
            for (auto it1 = it0; it1 != StaticWSMap.end(); it1++)
            {
                // don't intersect kernels with themselves
                if (it0->first == it1->first)
                {
                    continue;
                }

                /// Create a new entry in the ProdConRelationships vector
                auto newPoint = ProdCon();
                newPoint.kernels.first = it0->first;
                newPoint.kernels.second = it1->first;

                /// it0->input working set & it1->output working set
                vector<uint64_t> intersect;
                if (it0->second.input.size() > it1->second.output.size())
                {
                    intersect = vector<uint64_t>(it0->second.input.size());
                }
                else
                {
                    intersect = vector<uint64_t>(it1->second.output.size());
                }
                auto it = set_intersection(it0->second.input.begin(), it0->second.input.end(), it1->second.output.begin(), it1->second.output.end(), intersect.begin());
                intersect.resize(it - intersect.begin());
                newPoint.InputOutput.insert(intersect.begin(), intersect.end());

                /// it0->internal working set & it1->internal working set
                intersect.clear();
                if (it0->second.internal.size() > it1->second.internal.size())
                {
                    intersect = vector<uint64_t>(it0->second.internal.size());
                }
                else
                {
                    intersect = vector<uint64_t>(it1->second.internal.size());
                }
                it = set_intersection(it0->second.internal.begin(), it0->second.internal.end(), it1->second.internal.begin(), it1->second.internal.end(), intersect.begin());
                intersect.resize(it - intersect.begin());
                newPoint.InternalInternal.insert(intersect.begin(), intersect.end());

                /// it0->output working set & it1->input working set
                intersect.clear();
                if (it0->second.output.size() > it1->second.input.size())
                {
                    intersect = vector<uint64_t>(it0->second.output.size());
                }
                else
                {
                    intersect = vector<uint64_t>(it1->second.input.size());
                }
                it = set_intersection(it0->second.output.begin(), it0->second.output.end(), it1->second.input.begin(), it1->second.input.end(), intersect.begin());
                intersect.resize(it - intersect.begin());
                newPoint.OutputInput.insert(intersect.begin(), intersect.end());

                // insert newPoint into the global vector
                ProdConRelationships.push_back(newPoint);
            }
        }
    }

    /// Maps a time in the trace to the birth or death time of an address
    map<uint64_t, uint64_t, greater<>> BirthTimeMap;
    map<uint64_t, uint64_t, greater<>> DeathTimeMap;
    /// Maps a kernel ID to the max live address counts of each working set and total
    map<int, struct DynamicSets> DynamicWSMap;
    void CreateDynamicSets(bool nobar)
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
        /// Initialize DynamicWSMap
        for (const auto &kernelID : AddressSetMap)
        {
            // max count map
            DynamicWSMap[kernelID.first] = DynamicSets();
            // the input working set is all alive at time 0
            DynamicWSMap[kernelID.first].inputMax = StaticWSMap[kernelID.first].input.size();
            DynamicWSMap[kernelID.first].internalMax = 0;
            DynamicWSMap[kernelID.first].outputMax = 0;
            DynamicWSMap[kernelID.first].totalMax = StaticWSMap[kernelID.first].input.size();
            // live address set map
            // input working set must be pre-populated (because all input addresses are alive at the start of the program)
            DynamicWSMap[kernelID.first].input.insert(StaticWSMap[kernelID.first].input.begin(), StaticWSMap[kernelID.first].input.end());
            // TODO: the internal address set needs to be primed with internal addresses that are first read from, but the current implementation can't do this
            // The total set also has to be pre-populated with the internal working set
            DynamicWSMap[kernelID.first].total.insert(StaticWSMap[kernelID.first].input.begin(), StaticWSMap[kernelID.first].input.end());
        }
        vector<int> currentKernels = vector<int>();
        // go through the entire trace time
        for (uint64_t timeCount = 0; timeCount < maxTime + 1; timeCount++)
        {
            // find the map that has our timestamp in it
            if (BirthTimeMap.find(timeCount) != BirthTimeMap.end())
            {
                // find the kernel IDs that have this address
                for (const auto &key : AddressSetMap)
                {
                    // if this address belongs to this kernel's store set, add the kernel to the list
                    if ((key.second.store.find(BirthTimeMap[timeCount]) != key.second.store.end()))
                    {
                        currentKernels.push_back(key.first);
                    }
                }
                // for each kernel our address belongs to, add it to the total live address set
                for (const auto &ind : currentKernels)
                {
                    DynamicWSMap[ind].total.insert(BirthTimeMap[timeCount]);
                    // since this is a birth address, it can't belong to the input working set of this kernel
                    // if this address belongs to the output working set
                    if (StaticWSMap[ind].output.find(BirthTimeMap[timeCount]) != StaticWSMap[ind].output.end())
                    {
                        DynamicWSMap[ind].output.insert(BirthTimeMap[timeCount]);
                    }
                    // if this address belongs to the internal working set
                    if (StaticWSMap[ind].internal.find(BirthTimeMap[timeCount]) != StaticWSMap[ind].internal.end())
                    {
                        DynamicWSMap[ind].internal.insert(BirthTimeMap[timeCount]);
                    }
                    // update our max counts
                    // input working set max can never be updated
                    if (DynamicWSMap[ind].internalMax < DynamicWSMap[ind].internal.size())
                    {
                        DynamicWSMap[ind].internalMax = DynamicWSMap[ind].internal.size();
                    }
                    if (DynamicWSMap[ind].outputMax < DynamicWSMap[ind].output.size())
                    {
                        DynamicWSMap[ind].outputMax = DynamicWSMap[ind].output.size();
                    }
                    if (DynamicWSMap[ind].totalMax < DynamicWSMap[ind].total.size())
                    {
                        DynamicWSMap[ind].totalMax = DynamicWSMap[ind].total.size();
                    }
                }
            }
            else if (DeathTimeMap.find(timeCount) != DeathTimeMap.end())
            {
                // find the kernel IDs that have this address
                for (const auto &key : AddressSetMap)
                {
                    // if this address belongs to the kernel's load set, add this kernel to the list
                    if (key.second.load.find(DeathTimeMap[timeCount]) != key.second.load.end())
                    {
                        currentKernels.push_back(key.first);
                    }
                }
                for (const auto &ind : currentKernels)
                {
                    DynamicWSMap[ind].total.erase(DeathTimeMap[timeCount]);
                    // if this address belongs to the input working set
                    if (StaticWSMap[ind].input.find(DeathTimeMap[timeCount]) != StaticWSMap[ind].input.end())
                    {
                        DynamicWSMap[ind].input.erase(DeathTimeMap[timeCount]);
                    }
                    // if this address belongs to the internal working set
                    if (StaticWSMap[ind].internal.find(DeathTimeMap[timeCount]) != StaticWSMap[ind].internal.end())
                    {
                        DynamicWSMap[ind].internal.erase(DeathTimeMap[timeCount]);
                    }
                    // since this is a death address, it can't belong to the output working set of this kernel
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