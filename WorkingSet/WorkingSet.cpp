#include "inc/WorkingSet.h"
#include <algorithm>
#include <iostream>

using namespace std;
namespace WorkingSet
{
    /// Maps a kernel index to its set of basic block IDs
    std::map<int, std::set<int64_t>> kernelBlockMap;
    void Setup(nlohmann::json &j)
    {
        for (auto &[k, l] : j["Kernels"].items())
        {
            int index = stoi(k);
            nlohmann::json kernel = l["Blocks"];
            kernelBlockMap[index] = kernel.get<set<int64_t>>();
        }
    }
    /// Maps a kernel index to a pair of sets (first -> ld addr, second -> st addr)
    std::map<int, std::pair<std::set<uint64_t>, std::set<uint64_t>>> kernelSetMap;
    vector<int> currentKernelIDs;
    void Process(std::string &key, std::string &value)
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
            for (const auto &ind : currentKernelIDs)
            {
                uint64_t addr = stoul(value, nullptr, 0);
                kernelSetMap[ind].first.insert(addr);
            }
        }
        else if (key == "StoreAddress")
        {
            for (const auto &ind : currentKernelIDs)
            {
                uint64_t addr = stoul(value, nullptr, 0);
                kernelSetMap[ind].second.insert(addr);
            }
        }
        else if (key == "BBExit")
        {
            currentKernelIDs.clear();
        }
    }

    /// Maps a kernel index to its internal working set
    std::map<int, std::set<uint64_t>> kernelIntSetMap;
    void InternalSet()
    {
        for (const auto &key : kernelSetMap)
        {
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
            kernelIntSetMap[key.first] = set<uint64_t>(intersect.begin(), intersect.end());
            cout << "The size of the internal working set for kernel ID " << key.first << " is " << kernelIntSetMap[key.first].size() << endl;
        }
    }

    void PrintOutput()
    {
        cout << "Outputting kernelSetMap" << endl;
        for (const auto &key : kernelSetMap)
        {
            cout << "The kernel index is: " << key.first << endl;
            cout << "The ld addrs are: " << endl;
            for (const auto &ind : key.second.first)
            {
                cout << ind << ",";
            }
            cout << "\nThe st addrs are: " << endl;
            for (const auto &ind : key.second.second)
            {
                cout << ind << ",";
            }
            cout << "\nThe internal addrs are " << endl;
            for (const auto &ind : kernelIntSetMap[key.first])
            {
                cout << ind << ",";
            }
        }
    }

    void PrintSizes()
    {
        cout << "Outputting kernelSetMap" << endl;
        for (const auto &key : kernelSetMap)
        {
            cout << "The kernel index is: " << key.first << ", its ld set size is " << key.second.first.size() << ", its st set size is " << key.second.second.size() << ", and its internal set size is " << kernelIntSetMap[key.first].size() << endl;
        }
    }
} // namespace WorkingSet