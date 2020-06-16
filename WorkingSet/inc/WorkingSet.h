#pragma once
#include <nlohmann/json.hpp>
#include <utility>
#include <map>
#include <set>

namespace WorkingSet
{
    /// Accepts an input nlohmann::json object to initialize global kernel block map
    void Setup(nlohmann::json&);
    /// Parses input trace into kernelSetMap
    void Process(std::string& , std::string&);
    void Print();
} // namespace WorkingSet