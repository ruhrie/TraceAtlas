


namespace WorkingSets
{
    /// Maps a kernel index to a pair of sets (first -> ld addr, second -> st addr)
    map<int, pair< set<uint64_t>, set<uint64_t> >> kernelSetMap; 
    /// Maps a kernel index to its set of basic block IDs
    map<int, int> kernelBlockMap;

} // namespace WorkingSets