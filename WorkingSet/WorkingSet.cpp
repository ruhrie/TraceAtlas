#include "inc/WorkingSet.h"
#include <algorithm>
#include <iostream>

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
    map<pair<int, int>, vector<set<uint64_t>>> prodConMap;
    void IntersectKernels()
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
                prodConMap[newPair] = vector<set<uint64_t>>(3);

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
                prodConMap[newPair][0] = set<uint64_t>(intersect.begin(), intersect.end());

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
                prodConMap[newPair][1] = set<uint64_t>(intersect.begin(), intersect.end());

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
                prodConMap[newPair][2] = set<uint64_t>(intersect.begin(), intersect.end());
            }
        }
    }

    void PrintOutput()
    {
        /*
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
        */
        for (const auto &key : kernelWSMap)
        {
            cout << "The kernel index is: " << key.first << endl;
            cout << "The input working set addrs are: " << endl;
            for (const auto &ind : key.second[0])
            {
                cout << ind << ",";
            }
            cout << "\nThe output working set addrs are: " << endl;
            for (const auto &ind : key.second[1])
            {
                cout << ind << ",";
            }
            cout << "\nThe internal working set addrs are " << endl;
            for (const auto &ind : key.second[2])
            {
                cout << ind << ",";
            }
        }
    }

    void PrintSizes()
    {
        /*cout << "Outputting kernelSetMap" << endl;
        for (const auto &key : kernelSetMap)
        {
            cout << "The kernel index is: " << key.first << ", its ld set size is " << key.second.first.size() << ", its st set size is " << key.second.second.size() << ", and its internal set size is " << kernelWSMap[key.first].size() << endl;
        }*/
        cout << "Outputting kernelWSMap" << endl;
        for (const auto &key : kernelWSMap)
        {
            cout << "The kernel index is: " << key.first << ", its input working set size is " << key.second[0].size() << ", its internal working set size is " << key.second[1].size() << ", and its output working set size is " << key.second[2].size() << endl;
        }
        cout << "Outputting prodConMap" << endl;
        for (const auto &key : prodConMap)
        {
            cout << "The kernel pair is: " << key.first.first << "," << key.first.second << ", its input-output intersection size is " << key.second[0].size() << ", its internal intersection size is " << key.second[1].size() << ", and its output-input intersection set size is " << key.second[2].size() << endl;
        }
    }
} // namespace WorkingSet