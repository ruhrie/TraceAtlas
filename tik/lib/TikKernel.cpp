#include "tik/TikKernel.h"
#include "AtlasUtil/Exceptions.h"
#include <nlohmann/json.hpp>

using namespace llvm;
using namespace std;

namespace TraceAtlas::tik
{
    TikKernel::TikKernel(Function *kernFunc)
    {
        KernelFunction = kernFunc;
        Name = KernelFunction->getName();
        // TODO: Conditional init

        // get the metadata and parse it into entrance, exit, values
        //     turn metadata into string
        //     parse json and populate Entrances, Exits,
        MDNode *meta = KernelFunction->getMetadata("Boundaries");
        std::string metaString;
        if (auto mstring = dyn_cast<MDString>(meta->getOperand(0)))
        {
            metaString = mstring->getString();
        }
        else
        {
            AtlasException("Could not convert metadata into string.");
        }
        nlohmann::json js = nlohmann::json::parse(metaString);
        // initialize Entrances, Exits
        for (int j = 0; j < (int)js["Entrances"]["Blocks"].size(); j++)
        {
            auto n = make_shared<KernelInterface>(js["Entrances"]["Indices"][j], (uint)js["Entrances"]["Blocks"][j]);
            Entrances.insert(n);
        }
        for (int j = 0; j < (int)js["Exits"]["Blocks"].size(); j++)
        {
            auto n = make_shared<KernelInterface>(js["Exits"]["Indices"][j], (uint)js["Exits"]["Blocks"][j]);
            Exits.insert(n);
        }
        // initialize ArgumentMap
        for (int j = 0; j < (int)js["Arguments"].size(); j++)
        {
            ArgumentMap[KernelFunction->arg_begin() + j] = (int)js["Arguments"][j];
        }
        for (auto BB = KernelFunction->begin(); BB != KernelFunction->end(); BB++)
        {
            auto block = cast<BasicBlock>(BB);
            // Special class members
            if (block->getName() == "Init")
            {
                Init = block;
            }
            else if (block->getName() == "Exit")
            {
                Exit = block;
            }
            else if (block->getName() == "Exception")
            {
                Exception = block;
            }
            else
            {
                continue;
            }
        }
        Valid = true;
    }
} // namespace TraceAtlas::tik