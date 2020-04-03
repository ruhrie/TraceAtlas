#include "cartographer.h"
#include <algorithm>
#include <atomic>
#include <map>
#include <queue>
#include <set>
#include <string>

using namespace std;

namespace TypeOne
{
    std::map<int64_t, std::map<int64_t, uint64_t>> blockMap;
    std::map<int64_t, atomic<uint64_t>> blockCount;
    std::deque<int64_t> priorBlocks;
    uint32_t radius = 5;
    void Process(std::string &key, std::string &value)
    {
        if (key == "BBEnter")
        {
            long int block = stoi(value, nullptr, 0);
            blockCount[block] += 1;
            priorBlocks.push_back(block);

            if (priorBlocks.size() > (2 * radius + 1))
            {
                priorBlocks.pop_front();
            }
            if (priorBlocks.size() > radius)
            {
                for (auto i : priorBlocks)
                {
                    blockMap[block][i]++;
                }
            }
        }
    }

    std::set<std::set<int64_t>> Get()
    {
        std::map<int64_t, std::vector<std::pair<int64_t, float>>> fBlockMap;
        for (auto &key : blockMap)
        {
            uint64_t total = 0;
            for (auto &sub : key.second)
            {
                total += sub.second;
            }
            for (auto &sub : key.second)
            {
                float val = (float)sub.second / (float)total;
                fBlockMap[key.first].push_back(std::pair<int64_t, float>(sub.first, val));
            }
        }

        std::set<int64_t> covered;
        std::vector<std::set<int64_t>> kernels;

        std::vector<std::pair<int64_t, int64_t>> blockPairs;
        blockPairs.reserve(blockCount.size());
        for (auto &iter : blockCount)
        {
            blockPairs.emplace_back(iter);
        }

        std::sort(blockPairs.begin(), blockPairs.end(), [=](std::pair<int64_t, int64_t> &a, std::pair<int64_t, int64_t> &b) {
            bool result;
            if (a.second > b.second)
            {
                result = true;
            }
            else if (a.second == b.second && a.first < b.first)
            {
                result = true;
            }
            else if (a.second == b.second && a.first >= b.first)
            {
                result = false;
            }
            else
            {
                result = false;
            }
            return result;
        });
        for (auto &it : blockPairs)
        {
            if (it.second >= hotThreshold)
            {
                if (covered.find(it.first) == covered.end())
                {
                    float sum = 0.0;
                    vector<pair<int64_t, float>> values = fBlockMap[it.first];
                    std::sort(values.begin(), values.end(), [=](std::pair<int64_t, float> &a, std::pair<int64_t, float> &b) {
                        bool result;
                        if (a.second > b.second)
                        {
                            result = true;
                        }
                        else if (a.second == b.second && a.first < b.first)
                        {
                            result = true;
                        }
                        else if (a.second == b.second && a.first >= b.first)
                        {
                            result = false;
                        }
                        else
                        {
                            result = false;
                        }
                        return result;
                    });
                    std::set<int64_t> kernel;
                    while (sum < threshold)
                    {
                        std::pair<int64_t, float> entry = values[0];
                        covered.insert(entry.first);
                        std::remove(values.begin(), values.end(), entry);
                        sum += entry.second;
                        kernel.insert(entry.first);
                    }
                    kernels.push_back(kernel);
                }
            }
            else
            {
                break;
            }
        }

        std::set<std::set<int64_t>> result;
        for (auto &it : kernels)
        {
            result.insert(it);
        }
        return result;
    }

    void Reset()
    {
        priorBlocks.clear();
    }
} // namespace TypeOne
