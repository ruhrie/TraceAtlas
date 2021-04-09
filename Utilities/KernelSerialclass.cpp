#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Traces.h"
#include <algorithm>
#include <fstream>
#include <indicators/progress_bar.hpp>
#include <iostream>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <zlib.h>

typedef struct wsTuple
{
    uint64_t start;
    uint64_t end;
    uint64_t byte_count;
    uint64_t ref_count;
    float regular;
    uint64_t timing;
} wsTuple;


class Tuple
{
private:
    /* data */
    uint64_t start;
    uint64_t end;
    uint64_t byte_count;
    uint64_t ref_count;
    float reuseDistance;
    uint64_t timing;
public:
    Tuple(wsTuple * inputParameters);
    Tuple();
};
 Tuple: Tuple(wsTuple * inputParameters)
{
    start = in

}
 Tuple:: Tuple()
{

}
