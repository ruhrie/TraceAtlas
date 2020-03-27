#include "cartographer.h"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>

using namespace std;

namespace TypeOne
{
    std::map<int, std::map<int, uint64_t>> blockMap;
    std::map<int, uint64_t> blockCount;
    std::deque<int> priorBlocks;
    uint32_t radius = 5;
    void Process(std::string &key, std::string &value)
    {
        if (key == "BBEnter")
        {
            long int block = stoi(value, 0, 0);
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

    std::set<std::set<int>> Get()
    {
        std::map<int, std::vector<std::pair<int, float>>> fBlockMap;
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
                fBlockMap[key.first].push_back(std::pair<int, float>(sub.first, val));
            }
        }

        std::set<int> covered;
        std::vector<std::set<int>> kernels;

        std::vector<std::pair<int, int>> blockPairs;
        for (auto iter = blockCount.begin(); iter != blockCount.end(); iter++)
        {
            blockPairs.push_back(*iter);
        }

        std::sort(blockPairs.begin(), blockPairs.end(), [=](std::pair<int, int> &a, std::pair<int, int> &b) {
            if (a.second > b.second)
            {
                return true;
            }
            else if (a.second == b.second)
            {
                if (a.first < b.first)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        });
        for (auto &it : blockPairs)
        {
            if (it.second >= hotThreshold)
            {
                if (covered.find(it.first) == covered.end())
                {
                    float sum = 0.0;
                    vector<pair<int, float>> values = fBlockMap[it.first];
                    std::sort(values.begin(), values.end(), [=](std::pair<int, float> &a, std::pair<int, float> &b) {
                        if (a.second > b.second)
                        {
                            return true;
                        }
                        else if (a.second == b.second)
                        {
                            if (a.first < b.first)
                            {
                                return true;
                            }
                            else
                            {
                                return false;
                            }
                        }
                        else
                        {
                            return false;
                        }
                    });
                    std::set<int> kernel;
                    while (sum < threshold)
                    {
                        std::pair<int, float> entry = values[0];
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

        std::set<std::set<int>> result;
        for (auto &it : kernels)
        {
            result.insert(it);
        }
        return result;
    }
} // namespace TypeOne
