#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <utility>

namespace WorkingSet
{
    /// Holds the load address and store address sets of each kernel index
    struct AddressSets
    {
        std::set<uint64_t> load;
        std::set<uint64_t> store;
        AddressSets()
        {
            load = std::set<uint64_t>();
            store = std::set<uint64_t>();
        }
    };

    /// Holds the three static working sets of each kernel index
    struct StaticSets
    {
        std::set<uint64_t> input;
        std::set<uint64_t> internal;
        std::set<uint64_t> output;
        StaticSets()
        {
            input = std::set<uint64_t>();
            internal = std::set<uint64_t>();
            output = std::set<uint64_t>();
        }
    };

    /// Holds the dynamic live address sets of each kernel index and the maximum sizes they achieve when parsing the address space
    struct DynamicSets
    {
        uint64_t inputMax;
        uint64_t internalMax;
        uint64_t outputMax;
        uint64_t totalMax;
        std::set<uint64_t> input;
        std::set<uint64_t> internal;
        std::set<uint64_t> output;
        std::set<uint64_t> total;
        DynamicSets()
        {
            inputMax = 0;
            internalMax = 0;
            outputMax = 0;
            totalMax = 0;
            input = std::set<uint64_t>();
            internal = std::set<uint64_t>();
            output = std::set<uint64_t>();
            total = std::set<uint64_t>();
        }
    };

    /// Holds the producer-consumer sets
    struct ProdCon
    {
        /// The first entry in the pair corresponds to the first word in each set name
        /// Ex. OutputInput set is the intersection of kernels.first.output and kernels.second.input
        std::pair<int, int> kernels;
        std::set<uint64_t> OutputInput;
        std::set<uint64_t> InternalInternal;
        std::set<uint64_t> InputOutput;
        ProdCon()
        {
            kernels = std::pair<int, int>();
            OutputInput = std::set<uint64_t>();
            InternalInternal = std::set<uint64_t>();
            InputOutput = std::set<uint64_t>();
        }
    };

    /// Accepts an input nlohmann::json object to initialize global kernel block map
    void Setup(nlohmann::json &);
    /// Parses input trace into kernelSetMap
    void Process(std::string &, std::string &);
    void CreateStaticSets();
    void ProducerConsumer();
    void CreateDynamicSets(bool nobar);
    extern std::map<int, struct StaticSets> StaticWSMap;
    extern std::vector<struct ProdCon> ProdConRelationships;
    extern std::map<int, struct DynamicSets> DynamicWSMap;

} // namespace WorkingSet