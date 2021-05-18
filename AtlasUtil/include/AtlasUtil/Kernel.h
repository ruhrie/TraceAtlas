#pragma once
#include <algorithm>
#include <map>
#include <string>
#include <vector>

static std::map<std::string, std::vector<std::string>> GetKernelHeierarchy(std::map<std::string, std::vector<int64_t>> kernels)
{
    std::map<std::string, std::vector<std::string>> childParentMapping;
    for (auto element : kernels)
    {
        for (auto comparison : kernels)
        {
            if (element.first != comparison.first)
            {
                if (element.second < comparison.second)
                {
                    std::vector<int64_t> i;
                    std::set_intersection(element.second.begin(), element.second.end(), comparison.second.begin(), comparison.second.end(), back_inserter(i));
                    if (i == comparison.second)
                    {
                        childParentMapping[element.first].push_back(comparison.first);
                    }
                }
            }
        }
    }
    return childParentMapping;
}