#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <utility>

namespace WorkingSet
{
    /// Accepts an input nlohmann::json object to initialize global kernel block map
    void Setup(nlohmann::json &);
    /// Parses input trace into kernelSetMap
    void Process(std::string &, std::string &);
    void CreateSets();
    void IntersectKernels();
    void JohnsAlgorithm();
    void PrintOutput();
    void PrintSizes();
    extern std::map<int, std::vector<std::set<uint64_t>>> kernelWSMap;
    extern std::map<std::pair<int, int>, std::vector<std::set<uint64_t>>> ProdConMap;
    extern std::map<int, unsigned long> liveAddressMaxCounts;
    /// Maps a kernel index to a vector of unsigned longs, where each entry is the maximum live addr count of each working set
    /// 0 -> input live addr max count, 1 -> internal live addr max count, 2 -> output live addr max count
    extern std::map<int, std::vector<uint64_t>> kernelWSLiveAddrMaxCounts;

} // namespace WorkingSet