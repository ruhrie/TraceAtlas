#include <set>
#include <string>

using namespace std;

namespace TypeOne
{
    void Process(std::vector<std::string> &values);
    std::set<std::set<int64_t>> Get();
    extern std::map<int64_t, uint64_t> blockCount;
} // namespace TypeOne