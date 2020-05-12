#include "TypeFour.h"
#include "AtlasUtil/Annotate.h"
#include "AtlasUtil/Exceptions.h"
#include "AtlasUtil/Print.h"
#include "cartographer.h"
#include "tik/Util.h"
#include <indicators/progress_bar.hpp>
#include <queue>
#include <spdlog/spdlog.h>

using namespace std;
using namespace llvm;

namespace TypeFour
{
    //this was taken from tik. It should probably be moved to libtik once it is made

    set<set<int64_t>> Process(const set<set<int64_t>> &type3Kernels)
    {
        indicators::ProgressBar bar;
        if (!noBar)
        {
            bar.set_option(indicators::option::PrefixText{"Detecting type 4 kernels"});
            bar.set_option(indicators::option::ShowElapsedTime{true});
            bar.set_option(indicators::option::ShowRemainingTime{true});
            bar.set_option(indicators::option::BarWidth{50});
        }

        uint64_t total = type3Kernels.size();
        int status = 0;

        set<set<int64_t>> result;
        for (const auto &kernel : type3Kernels)
        {
            set<int64_t> blocks;
            for (auto block : kernel)
            {
                //we need to see if this block can ever reach itself
                BasicBlock *base = blockMap[block];
                if (TraceAtlas::tik::IsSelfReachable(base, kernel))
                {
                    blocks.insert(block);
                }
            }
            //blocks is now a set, but it may be disjoint, so we need to check that now

            set<BasicBlock *> blockSet;
            for (auto block : blocks)
            {
                blockSet.insert(blockMap[block]);
            }

            set<BasicBlock *> entrances = TraceAtlas::tik::GetEntrances(blockSet);

            for (auto ent : entrances)
            {
                auto a = TraceAtlas::tik::GetReachable(ent, blocks);
                set<int64_t> b;
                for (auto as : a)
                {
                    b.insert(GetBlockID(as));
                }
                result.insert(b);
            }
            status++;
        }

        if (!noBar && !bar.is_completed())
        {
            bar.set_option(indicators::option::PostfixText{"Kernel " + to_string(status) + "/" + to_string(total)});
            bar.set_progress(100);
            bar.mark_as_completed();
        }

        return result;
    }
} // namespace TypeFour