#include "AtlasUtil/Format.h"
#include "AtlasUtil/IO.h"
#include "Dijkstra.h"
#include "Kernel.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <llvm/Support/CommandLine.h>
#include <queue>

using namespace llvm;
using namespace std;
using namespace TraceAtlas::Cartographer;
using json = nlohmann::json;

cl::opt<string> InputFilename("i", cl::desc("Specify bin file"), cl::value_desc(".bin filename"), cl::Required);
cl::opt<string> BitcodeFileName("b", cl::desc("Specify bitcode file"), cl::value_desc(".bc filename"), cl::Required);
cl::opt<string> BlockInfoFilename("bi", cl::desc("Specify BlockInfo.json file"), cl::value_desc(".json filename"), cl::Required);
cl::opt<string> DotFile("d", cl::desc("Specify dot filename"), cl::value_desc("dot file"));
cl::opt<string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("kernel filename"), cl::Required);

void ProfileBlock(BasicBlock *BB, map<int64_t, map<string, uint64_t>> &rMap, map<int64_t, map<string, uint64_t>> &cpMap)
{
    int64_t id = GetBlockID(BB);
    for (auto bi = BB->begin(); bi != BB->end(); bi++)
    {
        auto *i = cast<Instruction>(bi);
        if (i->getMetadata("TikSynthetic") != nullptr)
        {
            continue;
        }
        //start with the opcodes
        string name = string(i->getOpcodeName());
        rMap[id][name + "Count"]++;
        //now check the type
        Type *t = i->getType();
        if (t->isVoidTy())
        {
            rMap[id]["typeVoid"]++;
            cpMap[id][name + "typeVoid"]++;
        }
        else if (t->isFloatingPointTy())
        {
            rMap[id]["typeFloat"]++;
            cpMap[id][name + "typeFloat"]++;
        }
        else if (t->isIntegerTy())
        {
            rMap[id]["typeInt"]++;
            cpMap[id][name + "typeInt"]++;
        }
        else if (t->isArrayTy())
        {
            rMap[id]["typeArray"]++;
            cpMap[id][name + "typeArray"]++;
        }
        else if (t->isVectorTy())
        {
            rMap[id]["typeVector"]++;
            cpMap[id][name + "typeVector"]++;
        }
        else if (t->isPointerTy())
        {
            rMap[id]["typePointer"]++;
            cpMap[id][name + "typePointer"]++;
        }
        else
        {
            std::string str;
            llvm::raw_string_ostream rso(str);
            t->print(rso);
            cerr << "Unrecognized type: " + str + "\n";
        }
        rMap[id]["instructionCount"]++;
        cpMap[id]["instructionCount"]++;
    }
}

map<string, map<string, map<string, int>>> ProfileKernels(const std::map<string, std::set<int64_t>> &kernels, Module *M, std::map<int64_t, uint64_t> &blockCounts)
{
    map<int64_t, map<string, uint64_t>> rMap;  //dictionary which keeps track of the actual information per block
    map<int64_t, map<string, uint64_t>> cpMap; //dictionary which keeps track of the cross product information per block
    //start by profiling every basic block
    for (auto &F : *M)
    {
        for (Function::iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
        {
            ProfileBlock(cast<BasicBlock>(BB), rMap, cpMap);
        }
    }

    // maps kernel ID to type of pi to instruction type to instruction count
    map<string, map<string, map<string, int>>> fin;

    map<string, map<string, int>> cPigData;  //from the trace
    map<string, map<string, int>> pigData;   //from the bitcode
    map<string, map<string, int>> ecPigData; //cross product from the trace
    map<string, map<string, int>> epigData;  //cross product from the bitcode

    for (const auto &kernel : kernels)
    {
        string iString = kernel.first;
        auto blocks = kernel.second;
        for (auto block : blocks)
        {
            // frequency count of this block
            uint64_t count = blockCounts[block];
            for (const auto &pair : rMap[block])
            {
                cPigData[iString][pair.first] += pair.second * count;
                pigData[iString][pair.first] += pair.second;
            }

            for (const auto &pair : cpMap[block])
            {
                ecPigData[iString][pair.first] += pair.second * count;
                epigData[iString][pair.first] += pair.second;
            }
        }
    }

    // now do kernel ID wise mapping
    for (const auto &kernelID : pigData)
    {
        fin[kernelID.first]["Pig"] = kernelID.second;
    }
    for (const auto &kernelID : cPigData)
    {
        fin[kernelID.first]["CPig"] = kernelID.second;
    }
    for (const auto &kernelID : epigData)
    {
        fin[kernelID.first]["EPig"] = kernelID.second;
    }
    for (const auto &kernelID : ecPigData)
    {
        fin[kernelID.first]["ECPig"] = kernelID.second;
    }
    return fin;
}

string GenerateDot(const set<GraphNode *, p_GNCompare> &nodes, const set<Kernel *, KCompare> &kernels)
{
    string dotString = "digraph{\n";
    /*int j = 0;
    // here we build the kernel group clusters
    for (const auto &kernel : kernels)
    {
        dotString += "\tsubgraph cluster_" + to_string(j) + "{\n";
        dotString += "\t\tlabel=\"Kernel " + to_string(j++) + "\";\n";
        for (auto b : kernel->getBlocks())
        {
            dotString += "\t\t" + to_string(b) + ";\n";
        }
        dotString += "\t}\n";
    }*/
    // label kernels
    for (const auto &kernel : kernels)
    {
        dotString += "\t" + to_string(kernel->virtualNode->NID) + " [label=\"" + kernel->Label + "\"]\n";
    }
    // now build out the nodes in the graph
    for (const auto &node : nodes)
    {
        for (const auto &n : node->neighbors)
        {
            dotString += "\t" + to_string(node->NID) + " -> " + to_string(n.first) + ";\n";
        }
        if (auto VKN = dynamic_cast<VKNode *>(node))
        {
            for (const auto &p : VKN->kernel->parentKernels)
            {
                dotString += "\t" + to_string(node->NID) + " -> " + to_string(p) + " [style=dashed];\n";
            }
        }
        /*for (auto bi = block->begin(); bi != block->end(); bi++)
        {
            if (auto *ci = dyn_cast<CallInst>(bi))
            {
                auto *F = ci->getCalledFunction();
                if (F != nullptr && !F->empty())
                {
                    BasicBlock *entry = &F->getEntryBlock();
                    auto id = GetBlockID(entry);
                    dotString += "\t" + to_string(b) + " -> " + to_string(id) + " [style=dashed];\n";
                }
            }
        }*/
    }
    dotString += "}";

    return dotString;
}

void AddNode(std::set<GraphNode *, p_GNCompare> &nodes, const GraphNode &newNode)
{
    nodes.insert(new GraphNode(newNode));
}

void AddNode(std::set<GraphNode *, p_GNCompare> &nodes, const VKNode &newNode)
{
    nodes.insert(new VKNode(newNode));
}

void RemoveNode(std::set<GraphNode *, p_GNCompare> &CFG, GraphNode *removeNode)
{
    // first, remove the node in question from any kernels it may belong to
    // set of kernels that are updated versions of existing kernels, each member will eventually replace the old one in the kernels (input arg) set
    /*for (const auto &kernel : kernels)
    {
        if (kernel->nodes.find(removeNode) != kernel->nodes.end())
        {
            // remove the node from the kernel
            kernel->nodes.erase(removeNode);
        }
    }*/
    // second, look for any VKNodes in the CFG and update their node sets if applicable
    /*set<VKNode,  p_GNCompare> newVKNodes;
    for (auto node : CFG)
    {
        //if( auto VKNode = dynamic_pointer_cast<struct VKNode>(sharedNode) )
        if (auto VKN = dynamic_cast<VKNode *>(node))
        {
            VKN->nodes.erase(removeNode);
        }
    }*/

    // third, remove the node from the graph and update the neighbors of the predecessors and the predecessors of the neighbors
    /*for( const auto& predID : removeNode->predecessors )
    {
        auto pred = CFG.find(predID);
        if( pred != CFG.end() )
        {
            (*pred)->neighbors.erase(removeNode->NID); 
            for( const auto& neighbor : removeNode->neighbors )
            {
                (*pred)->neighbors[neighbor.first] = neighbor.second;
            }
        }
    }
    for( const auto& neighborID : removeNode->neighbors )
    {
        auto neighbor = CFG.find(neighborID.first);
        if( neighbor != CFG.end() )
        {
            (*neighbor)->predecessors.erase(removeNode->NID);
            for( const auto& predID : removeNode->predecessors )
            {
                (*neighbor)->predecessors.insert(predID);
            }
        }
    }*/
    // fourth, free
    auto entry = CFG.find(removeNode->NID);
    if (entry != CFG.end())
    {
        CFG.erase(entry);
        delete removeNode;
    }
}

void RemoveNode(std::set<GraphNode *, p_GNCompare> &CFG, const GraphNode &removeNode)
{
    // fourth, free
    auto entry = CFG.find(removeNode.NID);
    if (entry != CFG.end())
    {
        CFG.erase(CFG.find(removeNode.NID));
        delete *entry;
    }
}

// this function is only written to support MARKOV_ORDER=1 (TODO: generalize source,sink reading)
int ReadBIN(std::set<GraphNode *, p_GNCompare> &nodes, const std::string &filename, bool print = false)
{
    // first initialize the graph to all the blocks in the
    FILE *f = fopen(filename.data(), "rb");
    if (!f)
    {
        return 1;
    }
    // first word is a uint32_t of the markov order of the graph
    uint32_t markovOrder;
    fread(&markovOrder, sizeof(uint32_t), 1, f);
    // second word is a uint32_t of the total number of blocks in the graph (each block may or may not be connected to the rest of the graph)
    uint32_t blocks;
    fread(&blocks, sizeof(uint32_t), 1, f);
    for (uint32_t i = 0; i < blocks; i++)
    {
        auto newNode = GraphNode(i);
        newNode.blocks.insert((int64_t)i);
        AddNode(nodes, newNode);
    }
    // third word is a uint32_t of how many edges there are in the file
    uint32_t edges;
    fread(&edges, sizeof(uint32_t), 1, f);

    // read all the edges
    // for now, only works when MARKOV_ORDER=1
    uint32_t source = 0;
    uint32_t sink = 0;
    uint64_t frequency = 0;
    // the __TA_element union contains 2 additional words for blockLabel char*
    uint64_t garbage = 0;
    for (uint32_t i = 0; i < edges; i++)
    {
        fread(&source, sizeof(uint32_t), 1, f);
        fread(&sink, sizeof(uint32_t), 1, f);
        fread(&frequency, sizeof(uint64_t), 1, f);
        fread(&garbage, sizeof(uint64_t), 1, f);

        auto nodeIt = nodes.find(source);
        if (nodeIt == nodes.end())
        {
            throw AtlasException("Found a node described in an edge that does not exist in the BBID space!");
        }

        if ((*nodeIt)->neighbors.find((uint64_t)sink) != (*nodeIt)->neighbors.end())
        {
            throw AtlasException("Found a sink node that is already a neighbor of this source node!");
        }
        (*nodeIt)->neighbors[(uint64_t)sink].first = frequency;
    }
    fclose(f);

    // calculate all the edge probabilities
    for (auto &node : nodes)
    {
        uint64_t sum = 0;
        for (const auto &successor : node->neighbors)
        {
            sum += successor.second.first;
        }
        for (auto &successor : node->neighbors)
        {
            successor.second.second = (double)successor.second.first / (double)sum;
        }
    }

    // fill in all the predecessor nodes
    for (auto &node : nodes)
    {
        for (const auto &neighbor : node->neighbors)
        {
            auto successorNode = nodes.find(neighbor.first);
            if (successorNode != nodes.end())
            {
                (*successorNode)->predecessors.insert(node->NID);
            }
            // the profile doesn't include the terminating block of the program (because it has no edges leading from it)
            // But this creates a problem when defining kernel exits, so look for the node who has a neighbor that is not in the graph already and add that neighbor (with correct predecessor)
            else
            {
                // we likely found the terminating block, so add the block and assign the current node to be its predecessor
                auto programTerminator = GraphNode(neighbor.first);
                programTerminator.predecessors.insert(node->NID);
                AddNode(nodes, programTerminator);
            }
        }
    }

    // finally, look for all nodes with no predecessors and no successors and remove them (they slow down the transform algorithms)
    vector<GraphNode *> toRemove;
    for (auto &node : nodes)
    {
        auto nodeObject = *node;
        if (nodeObject.predecessors.empty() && nodeObject.neighbors.empty())
        {
            toRemove.push_back(node);
        }
    }
    for (const auto &r : toRemove)
    {
        RemoveNode(nodes, r);
    }

    if (print)
    {
        for (const auto &node : nodes)
        {
            spdlog::info("Examining node " + std::to_string(node->NID));
            std::string preds;
            for (auto pred : node->predecessors)
            {
                preds += std::to_string(pred);
                if (pred != *prev(node->predecessors.end()))
                {
                    preds += ",";
                }
            }
            spdlog::info("Predecessors: " + preds);
            for (const auto &neighbor : node->neighbors)
            {
                spdlog::info("Neighbor " + std::to_string(neighbor.first) + " has instance count " + std::to_string(neighbor.second.first) + " and probability " + std::to_string(neighbor.second.second));
            }
            std::cout << std::endl;
        }
    }
    return 0;
}

/// Returns true if one or more cycles exist in the graph specified by nodes, false otherwise
bool FindCycles(const std::set<GraphNode *, p_GNCompare> &nodes, const GraphNode *source)
{
    // algorithm inspired by https://www.baeldung.com/cs/detecting-cycles-in-directed-graph
    // queue of nodes that have been touched but their neighbors have not been fully evaluated yet
    deque<const GraphNode *> Q;
    // set of nodes that are DONE, aka all edges leading from them have been visited
    set<const GraphNode *, p_GNCompare> done;
    // flag in which the DFS may flip if a cycle is found
    bool cycle = false;
    // start with the source
    Q.push_front(source);
    while (!Q.empty())
    {
        // flag that tracks whether or not the node in the front of the queue pushes a new entry to the queue
        // if it doesn't the node is done, else it stays in the queue
        bool pushed = false;
        for (const auto &n : Q.front()->neighbors)
        {
            if (nodes.find(n.first) == nodes.end())
            {
                // this node is outside the boundaries of the subgraph, skip
                continue;
            }
            auto neighbor = *nodes.find(n.first);
            bool inqueue = false;
            for (const auto &entry : Q)
            {
                if (entry == neighbor)
                {
                    inqueue = true;
                    break;
                }
            }
            if (inqueue)
            {
                // we have found a cycle
                cycle = true;
                break;
            }
            if (done.find(neighbor) == done.end())
            {
                Q.push_front(neighbor);
                pushed = true;
                // process neighbors one at a time, we will eventually circle back to this node in the queue
                break;
            }
        }
        if (!pushed)
        {
            done.insert(Q.front());
            Q.pop_front();
        }
    }
    return cycle;
}

void TrivialTransforms(std::set<GraphNode *, p_GNCompare> &nodes, std::map<int64_t, llvm::BasicBlock *> &IDToBlock)
{
    // a trivial node merge must satisfy two conditions
    // 1.) The source node has exactly 1 neighbor with certain probability
    // 2.) The sink node has exactly 1 predecessor (the source node) with certain probability
    // 3.) The edge connecting source and sink must not cross a context level i.e. source and sink must belong to the same function
    // combine all trivial edges
    vector<GraphNode *> tmpNodes(nodes.begin(), nodes.end());
    for (auto &node : tmpNodes)
    {
        if (nodes.find(node->NID) == nodes.end())
        {
            // we've already been removed, do nothing
            continue;
        }
        auto currentNode = *nodes.find(node->NID);
        while (true)
        {
            // first condition, our source node must have 1 certain successor
            if ((currentNode->neighbors.size() == 1) && (currentNode->neighbors.begin()->second.second > 0.9999))
            {
                auto succ = nodes.find(currentNode->neighbors.begin()->first);
                if (succ != nodes.end())
                {
                    // second condition, the sink node must have 1 certain predecessor
                    if (((*succ)->predecessors.size() == 1) && ((*succ)->predecessors.find(currentNode->NID) != (*succ)->predecessors.end()))
                    {
                        // third condition, edge must not cross a context level
                        auto sourceBlock = IDToBlock[(int64_t)currentNode->NID];
                        auto sinkBlock = IDToBlock[(int64_t)(*succ)->NID];
                        if (sourceBlock == nullptr || sinkBlock == nullptr)
                        {
                            throw AtlasException("Found a node in the graph whose ID did not map to a basic block pointer in the ID map!");
                        }
                        if (sourceBlock->getParent() == sinkBlock->getParent())
                        {
                            // trivial merge, we merge into the source node
                            // keep the NID, preds
                            // change the neighbors to the sink neighbors AND update the successors of the sink node to include the merged node in their predecessors
                            currentNode->neighbors.clear();
                            for (const auto &n : (*succ)->neighbors)
                            {
                                auto succ2 = nodes.find(n.first);
                                if (succ2 != nodes.end())
                                {
                                    currentNode->neighbors[n.first] = n.second;
                                    (*succ2)->predecessors.erase((*succ)->NID);
                                    (*succ2)->predecessors.insert(currentNode->NID);
                                }
                                else
                                {
                                    throw AtlasException("Successor missing from control flow graph!");
                                }
                            }
                            // add the successor blocks
                            currentNode->addBlocks((*succ)->blocks);

                            // remove stale node from the node set
                            RemoveNode(nodes, *succ);
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    throw AtlasException("Successor missing from control flow graph!");
                }
            }
            else
            {
                break;
            }
        }
    }
}

void BranchToSelectTransforms(std::set<GraphNode *, p_GNCompare> &nodes)
{
    // Vocabulary
    // entrance - first node that will execute in the target subgraph
    // midnodes - nodes that lie between entrance and exit
    // exit     - last node that will execute in the target subgraph
    // Rules
    // 1.) The subgraph must have exactly one entrance and one exit
    // 2.) Exactly one layer of midnodes must exist between entrance and exit. The entrance is allowed to lead directly to the exit
    // 3.) No cycles may exist in the subgraph i.e. Flow can only go from entrance to zero or one midnode to exit
    // 4.) No subgraph edges can cross context levels i.e. the entire subgraph must be contained in one function
    vector<GraphNode *> tmpNodes(nodes.begin(), nodes.end());
    for (auto &node : tmpNodes)
    {
        if (nodes.find(node->NID) == nodes.end())
        {
            // we've already been removed, do nothing
            continue;
        }
        auto entrance = *nodes.find(node->NID);
        while (true)
        {
            // block that may be an exit of a transformable subgraph
            auto potentialExit = nodes.end();

            // first step, acquire middle nodes
            // we do this by treating the current block as the entrance to a potential subgraph and exploring its neighbors and neighbors of neighbors
            set<uint64_t> midNodes;
            // also, check for case 1 of case 2 configurations
            // flag representing the case we have, if any.
            // false for Case 1, true for Case 2
            bool MergeCase = false;
            // 1.) 0-deep branch->select: entrance can go directly to exit
            // 2.) 1-deep branch->select: entrance cannot go directly to exit
            // holds all neighbors of all midnodes
            std::set<uint64_t> midNodeSuccessors;
            for (const auto &midNode : entrance->neighbors)
            {
                midNodes.insert(midNode.first);
                if (nodes.find(midNode.first) != nodes.end())
                {
                    for (const auto &neighbor : (*nodes.find(midNode.first))->neighbors)
                    {
                        midNodeSuccessors.insert(neighbor.first);
                    }
                }
                else
                {
                    throw AtlasException("Found a midnode that is not in the control flow graph!");
                }
            }
            if (midNodeSuccessors.size() == 1) // corner case where the exit of the subgraph has no successors (it is the last node to execute in the program). In this case we have to check if the entrance is a predecessor of the lone midNodeTarget
            {
                auto cornerCase = nodes.find(*midNodeSuccessors.begin());
                if (cornerCase != nodes.end())
                {
                    if ((*cornerCase)->predecessors.find(entrance->NID) != (*cornerCase)->predecessors.end())
                    {
                        // the entrance can lead directly to the exit
                        MergeCase = false;
                        potentialExit = cornerCase;
                        midNodes.erase((*potentialExit)->NID);
                    }
                    else
                    {
                        // we have confirmed case 2: all entrance successors lead to a common exit, meaning the entrance cannot lead directly to the exit
                        MergeCase = true;
                        potentialExit = nodes.find(*midNodeSuccessors.begin());
                    }
                }
                else
                {
                    throw AtlasException("Could not find midNode in control flow graph!");
                }
                if (potentialExit == nodes.end())
                {
                    break;
                }
            }
            // else we may have a neighbor of the entrance that is the exit
            // to find the exit we need to find a neighbor of the entrance which is the lone successor of all other neighbors of the entrance
            else
            {
                if (entrance->neighbors.size() > 1)
                {
                    for (auto succ = entrance->neighbors.begin(); succ != entrance->neighbors.end(); succ++)
                    {
                        bool common = true;
                        for (auto neighborID = entrance->neighbors.begin(); neighborID != entrance->neighbors.end(); neighborID++)
                        {
                            if (succ == neighborID)
                            {
                                continue;
                            }
                            auto neighbor = nodes.find(neighborID->first);
                            if (neighbor != nodes.end())
                            {
                                for (const auto &succ2 : (*neighbor)->neighbors)
                                {
                                    if (succ2.first != succ->first)
                                    {
                                        common = false;
                                    }
                                }
                            }
                            else
                            {
                                throw AtlasException("Neighbor not found in control flow graph!");
                            }
                        }
                        if (common)
                        {
                            potentialExit = nodes.find(succ->first);
                            midNodes.erase(succ->first);
                            break;
                        }
                    }
                }
                if (potentialExit == nodes.end())
                {
                    break;
                }
            }
            // in order for either case to be true, six conditions must be checked
            // 1.) the entrance can't have the exit or any midnodes as predecessors
            auto tmpMids = midNodes;
            auto pushed = tmpMids.insert((*potentialExit)->NID);
            if (!pushed.second)
            {
                // somehow the exit ID is still in the midNodes set
                throw AtlasException("PotentialExit found in the midNodes set!");
            }
            for (auto &pred : entrance->predecessors)
            {
                tmpMids.erase(pred);
            }
            if (tmpMids.size() != midNodes.size() + 1)
            {
                break;
            }
            // 2.) all midnodes must only have entrance as its lone predecessor
            bool badCondition = false; // can be flipped either by a bad midNode pred or a missing midNode from the nodes set
            for (const auto &mid : midNodes)
            {
                auto midNode = nodes.find(mid);
                if (midNode != nodes.end())
                {
                    if (((*midNode)->predecessors.size() != 1) || ((*midNode)->predecessors.find(entrance->NID) == (*midNode)->predecessors.end()))
                    {
                        badCondition = true;
                    }
                }
                else
                {
                    throw AtlasException("Missing midnode from the control flow graph!");
                }
            }
            if (badCondition)
            {
                break;
            }
            // 3.) all midnodes must only have potentialExit as its lone successor
            badCondition = false; // can be flipped either by a bad midNode pred or a missing midNode from the nodes set
            for (const auto &mid : midNodes)
            {
                auto midNode = nodes.find(mid);
                if (midNode != nodes.end())
                {
                    if (((*midNode)->neighbors.size() != 1) || ((*midNode)->neighbors.find((*potentialExit)->NID) == (*midNode)->neighbors.end()))
                    {
                        badCondition = true;
                    }
                }
                else
                {
                    throw AtlasException("Missing midnode from the control flow graph!");
                }
            }
            if (badCondition)
            {
                break;
            }
            // 4.) potentialExit can only have the midnodes as predecessors
            // BW [5/31/21] This is a case specific check and is done later
            /*tmpMids = midNodes;
            badCondition = false;
            for (auto &pred : (*potentialExit)->predecessors)
            {
                if (midNodes.find(pred) == midNodes.end())
                {
                    badCondition = true;
                    break;
                }
            }
            if (badCondition)
            {
                break;
            }*/
            // 5.) potentialExit can't have the entrance or any midnodes as successors
            tmpMids = midNodes;
            badCondition = false; // flipped if we find a midnode or entrance in potentialExit successors, or we find a bad node
            for (const auto &k : (*potentialExit)->neighbors)
            {
                if (midNodes.find(k.first) != midNodes.end())
                {
                    badCondition = true;
                    break;
                }
            }
            if (badCondition)
            {
                break;
            }
            // 6.) All nodes in the entire subgraph must be contained within a single function
            /* This check is turned off for now because it doesn't allow us to convert branch to select subgraphs that have trivially inlinable functions in them
            set<Function *> parents;
            for (const auto &block : entrance->blocks)
            {
                parents.insert(IDToBlock[block.first]->getParent());
            }
            for (const auto &mid : midNodes)
            {
                for (const auto &block : (*nodes.find(mid))->blocks)
                {
                    parents.insert(IDToBlock[block.first]->getParent());
                }
            }
            for (const auto &block : (*potentialExit)->blocks)
            {
                parents.insert(IDToBlock[block.first]->getParent());
            }
            if (parents.size() != 1)
            {
                // we have violated the condition that every block in the subgraph must belong to the same function, break
                break;
            }
            */
            // Now we do case-specific checks
            if (MergeCase)
            {
                // case 2: the entrance cannot lead directly to the exit
                // 2 conditions must be checked
                // 1.) entrance only has midnodes as successors
                tmpMids = midNodes;
                for (auto &n : entrance->neighbors)
                {
                    tmpMids.erase(n.first);
                }
                if (!tmpMids.empty())
                {
                    break;
                }
                // 2.) potentialExit only has midnodes as predecessors
                auto tmpPreds = (*potentialExit)->predecessors;
                for (auto &n : midNodes)
                {
                    tmpPreds.erase(n);
                }
                if (!tmpPreds.empty())
                {
                    break;
                }
            }
            else
            {
                // case 1: the entrance can lead directly to the exit
                // 2 conditions must be checked
                // 1.) entrance only has midnodes and potentialExit as successors
                tmpMids = midNodes;
                tmpMids.insert((*potentialExit)->NID);
                for (auto &n : entrance->neighbors)
                {
                    tmpMids.erase(n.first);
                }
                if (!tmpMids.empty())
                {
                    break;
                }
                // 2.) potentialExit only has midnodes and entrance as predecessors
                auto tmpPreds = (*potentialExit)->predecessors;
                tmpMids = midNodes;
                tmpMids.insert(entrance->NID);
                for (auto &n : tmpMids)
                {
                    tmpPreds.erase(n);
                }
                if (!tmpPreds.empty())
                {
                    break;
                }
            }
            // merge entrance, exit, thirdNode into the entrance
            // keep the NID, preds
            // change the neighbors to potentialExit neighbors AND update the successors of potentialExit to include the merged node in their predecessors
            entrance->neighbors.clear();
            for (const auto &n : (*potentialExit)->neighbors)
            {
                entrance->neighbors[n.first] = n.second;
                auto succ2 = nodes.find(n.first);
                if (succ2 != nodes.end())
                {
                    (*succ2)->predecessors.erase((*potentialExit)->NID);
                    (*succ2)->predecessors.insert(entrance->NID);
                }
                else
                {
                    throw AtlasException("Missing successor from the control flow graph!");
                }
            }
            // merge midNodes and potentialExit blocks in order
            for (auto n : midNodes)
            {
                auto midNode = nodes.find(n);
                if (midNode != nodes.end())
                {
                    entrance->addBlocks((*midNode)->blocks);
                }
                else
                {
                    throw AtlasException("Missing midNode from the control flow graph!");
                }
            }
            entrance->addBlocks((*potentialExit)->blocks);

            // remove stale nodes from the node set
            for (auto n : midNodes)
            {
                auto midNode = nodes.find(n);
                if (midNode != nodes.end())
                {
                    nodes.erase(*midNode);
                }
                else
                {
                    throw AtlasException("Missing midNode from the control flow graph!");
                }
            }
            RemoveNode(nodes, *potentialExit);
        }
    }
}

void FanInFanOutTransform(std::set<GraphNode *, p_GNCompare> &nodes)
{
    /// @brief Uses a breadth first search of the subgraph between the source node and sink node to determine whether all paths leading from source must lead through sink
    /// Queue:         The algorithm keeps a queue of nodes that are currently being searched.
    ///                push: whenever a new node is discovered
    ///                pop:  whenever all of a nodes predecessors and successors have been evaluated
    /// Subgraph:      A set of visited nodes that constitutes the subgraph between source and sink. A node must be a successor of the source node to be in the subgraph
    /// Steps:
    /// Initialization:
    ///   - Copy the entire CFG
    ///   - Push the source node to the queue
    ///   - while the queue is not empty...
    /// 1.) If the current node has already been visited
    ///   - this means there is a cycle, so quit
    /// 2.) If the current node is the sink node, the subgraph contains the two required characteristics:
    ///   a.) All paths leading from source must go through only the sink node (bottleneck)
    ///   b.) All paths leading into the subgraph msut go through only the source node (bottleneck)
    /// 3.) Visit each successor of the node in the front of the queue and add them to the subgraph set
    ///   - if one of these successors is the sink node:
    ///     -> this node can only have the sink node as a successor, otherwise the subgraph does not bottleneck at the sink node. Quit
    ///     -> this node can only have predecessors that are in the subgraph, otherwise there are paths not through the source node that can reach this point
    /// 4.) Check the predecessors of the current node
    ///   - if a predecessor is not in the subgraph
    ///     -> there is a path to here that does not go through the source node, quit
    ///
    vector<GraphNode *> tmpNodes(nodes.begin(), nodes.end());
    for (auto sourceIT = tmpNodes.begin(); sourceIT != tmpNodes.end(); sourceIT++)
    {
        auto source = *sourceIT;
        if (nodes.find(source->NID) == nodes.end())
        {
            // we've already been removed, do nothing
            continue;
        }
        // get the actual node from the graph
        source = *nodes.find(source->NID);
        for (auto sinkIT = next(sourceIT); sinkIT != tmpNodes.end(); sinkIT++)
        {
            auto sink = *sinkIT;
            if (nodes.find(sink->NID) == nodes.end())
            {
                // we've already been removed, do nothing
                continue;
            }
            // get the actual node form the graph
            sink = *nodes.find(sink->NID);

            // indicates whether all rules have been satisfied
            bool passed = true;
            // indicates whether the BFS found the sink node (aka there is a path between source and sink)
            bool sinkFound = false;
            // first check
            //  - All paths leading into the subgraph must go through source (bottleneck through source)
            //  - All paths leading out of the subgraph must go through sink (bottleneck through sink)
            set<GraphNode *, p_GNCompare> subgraph;
            deque<GraphNode *> Q;
            Q.push_front(source);
            while (!Q.empty())
            {
                if (Q.front() == sink)
                {
                    sinkFound = true;
                    // check if the sink is the only entry left in the queue
                    // if it is, we can check for a passing subgraph
                    // if it is not, we need to push it to the back of the queue and keep iterating (to ensure our graph is searched breadth first)
                    if (Q.size() == 1)
                    {
                        subgraph.insert(sink);
                        // first, check if all preds of the sink are in the subgraph. If one is missing, we have an entrance outside the subgraph. Quit
                        for (const auto &pred : sink->predecessors)
                        {
                            if (subgraph.find(pred) == subgraph.end())
                            {
                                passed = false;
                            }
                        }
                        // second, since we already checked all nodes between source and sink (breadth first), and we haven't broken yet, we've passed
                        // make sure the subgraph is doing more than just 1 node
                        /*if( subgraph.size() > 2 )
                        {
                            // not sure this is needed
                            passed = true;
                        }
                        else
                        {
                            passed = false;
                        }*/
                        break;
                    }
                    // there are still entries in the queue, so push the sink node to the back and iterate
                    Q.push_back(sink);
                    Q.pop_front();
                    continue;
                }
                subgraph.insert(Q.front());
                for (const auto &neighbor : Q.front()->neighbors)
                {
                    if (neighbor.first == sink->NID)
                    {
                        // this node can only have the sink as a neighbor
                        if (Q.front()->neighbors.size() != 1)
                        {
                            passed = false;
                        }
                        //Q.push_back( *nodes.find(neighbor.first) );
                    }
                    if (subgraph.find(neighbor.first) == subgraph.end())
                    {
                        bool found = false;
                        for (const auto &next : Q)
                        {
                            if (next->NID == neighbor.first)
                            {
                                found = true;
                            }
                        }
                        if (!found)
                        {
                            if (nodes.find(neighbor.first) == nodes.end())
                            {
                                throw AtlasException("Found a neighbor that does not exist within the graph!");
                            }
                            Q.push_back(*nodes.find(neighbor.first));
                        }
                    }
                    else
                    {
                        // we have found a cycle, quit
                        //passed = false;
                    }
                }
                for (const auto &pred : Q.front()->predecessors)
                {
                    if (subgraph.find(pred) == subgraph.end())
                    {
                        // this node has a pred that is not in the subgraph, if it is the source node this is allowed, otherwise the subgraph has an entrance that is not the source node. Quit
                        if (Q.front()->NID != source->NID)
                        {
                            passed = false;
                        }
                    }
                }
                if (!passed)
                {
                    break;
                }
                Q.pop_front();
            }

            // secong check: no cycles are allowed to exist within the subgraph
            if (FindCycles(subgraph, source))
            {
                break;
            }

            // third check: all function boundaries within the subgraph (if any) must be trivially inlinable

            // finally, if we passed all checks, condense the subgraph into the source node
            if (passed && sinkFound)
            {
                subgraph.erase(source);
                // condense all into the source node
                for (const auto &node : subgraph)
                {
                    source->addBlocks(node->blocks);
                }
                source->neighbors = sink->neighbors;
                for (const auto &nei : source->neighbors)
                {
                    auto neighbor = *nodes.find(nei.first);
                    if (nodes.find(nei.first) == nodes.end())
                    {
                        throw AtlasException("Found a neighbor that does not exist in the graph!");
                    }
                    neighbor->predecessors.erase(sink->NID);
                    neighbor->predecessors.insert(source->NID);
                }
                for (const auto &node : subgraph)
                {
                    nodes.erase(node);
                }
            }
        }
    }
}

std::vector<Kernel *> VirtualizeKernels(std::set<Kernel *, KCompare> &newKernels, std::set<GraphNode *, p_GNCompare> &nodes)
{
    vector<Kernel *> newPointers;
    for (const auto &kernel : newKernels)
    {
        try
        {
            // gather entrance and exit nodes
            auto kernelEntrances = kernel->getEntrances(nodes);
            auto kernelExits = kernel->getExits(nodes);
            if (kernelEntrances.empty() || kernelExits.empty())
            {
                continue;
            }
            // steps
            // first: select a node to become the VKNode (for now this is just the entrance at the front of the list) and remove all predecessors of the kernel entrance that are within the kernel itself (virtual kernel nodes are not allowed to loop onto themselves)
            // second: for each entrance, change the neighbor of each predecessor with the VKnode (this draws the existing edges from predecessor to kernel entrance - to the new VKNode)
            // third: collect all edges that lead out the kernel, and add all these edges to the VKnode neighbors (this draws existing edges from the new VKNode to its exit nodes)
            // fourth: remove all nodes within the kernel except any virtual kernels within this kernel (this deletes everything old in the graph, leaving only the new virtual kernel behind, but spares the child kernels which float in free space. They are used later to establish parent-child relationships)

            auto kernelNode = new VKNode(**kernelEntrances.begin(), kernel);
            // remove each node in the kernel that are possibly in the entrance predecessors
            // this removes all edges in the graph that can lead to the kernel other than the kernel entrance predecessors
            for (const auto &node : kernel->nodes)
            {
                kernelNode->predecessors.erase(node.NID);
            }

            // for each entrance, fix the neighbors of each predecessor
            //  - make new neighbor pointing to the VKnode (this will duplicate the neighbor for the original entrance node)
            //  - add this predecessor to the predecessors of the VKnode
            for (const auto &ent : kernelEntrances)
            {
                for (const auto &predID : ent->predecessors)
                {
                    auto pred = nodes.find(predID);
                    if (pred != nodes.end()) // sanity check
                    {
                        if (kernel->nodes.find(predID) == kernel->nodes.end()) // if this predecessor of the entrance is not a member of the kernel, proceed
                        {
                            (*pred)->neighbors[kernelNode->NID] = (*pred)->neighbors[ent->NID];
                            (*pred)->neighbors.erase(ent->NID);
                            kernelNode->predecessors.insert(predID);
                        }
                    }
                    else
                    {
                        throw AtlasException("Node predecessor not found in the control flow graph!");
                    }
                }
            }

            // for each exit, fix its predecessors
            //  - remove the original kernel node from the predecessors
            //  - add the VKnode to the predecessors
            kernelNode->neighbors.clear();
            for (const auto &exitNode : kernelExits)
            {
                // the only edges leading from the virtual kernel node should be edges out of the kernel
                // we are given the nodes inside the kernel that have successors outside the kernel, so here we figure out which exit edges from our exit node leave the kernel
                // we leave the probabilities the same as the original nodes (this means we will have outgoing edges whose probabilities do not sum to 1, because edges that go back in the kernel are omitted)
                for (const auto &en : exitNode->neighbors)
                {
                    // nodes within the kernel will not be in the neighbors of the virtual kernel
                    if (kernel->nodes.find(en.first) == kernel->nodes.end())
                    {
                        auto exitNeighbor = nodes.find(en.first);
                        if (exitNeighbor != nodes.end())
                        {
                            kernelNode->neighbors[en.first] = en.second;
                            (*exitNeighbor)->predecessors.erase(exitNode->NID);
                            (*exitNeighbor)->predecessors.insert(kernelNode->NID);
                        }
                        else
                        {
                            throw AtlasException("Node predecessor not found in the control flow graph!");
                        }
                    }
                }
            }
            // remove all nodes within the kernel except the entrance node and any virtual kernels within this kernel
            for (const auto &node : kernel->nodes)
            {
                auto it_Node = nodes.find(node.NID);
                if (it_Node != nodes.end())
                {
                    if (auto VKN = dynamic_cast<VKNode *>(*it_Node))
                    {
                        // don't throw away virtual kernel nodes, but disconnect them from the graph
                        VKN->kernel->parentKernels.insert((uint32_t)kernelNode->NID);
                        VKN->neighbors.clear();
                        VKN->predecessors.clear();
                        continue;
                    }
                }
                else
                {
                    throw AtlasException("Could not find kernel member in node graph!");
                }
                RemoveNode(nodes, node);
            }
            // finally replace the old entrance node with the new VKNode
            nodes.insert(kernelNode);
            kernel->virtualNode = kernelNode;
            newPointers.push_back(kernel);
        }
        catch (AtlasException &e)
        {
            spdlog::error(e.what());
        }
    }
    return newPointers;
}

double EntropyCalculation(std::set<GraphNode *, p_GNCompare> &nodes)
{
    // first, calculate the stationary distribution for each existing node
    // stationary distribution is the probability that I am in a certain state at any given time (it's an asymptotic measure)
    vector<double> stationaryDistribution(nodes.size(), 0.0);
    for (unsigned int i = 0; i < nodes.size(); i++)
    {
        auto it = nodes.begin();
        advance(it, i);
        // we sum along the columns (the probabilities of going to the current node), so we use the edge weight coming from each predecessor to this node
        for (const auto &pred : (*it)->predecessors)
        {
            auto predecessor = nodes.find(pred);
            // find the edge of the predecessor that goes to the current node
            for (const auto &nei : (*predecessor)->neighbors)
            {
                if (nei.first == (*it)->NID)
                {
                    // retrieve the edge probability and accumulate it to this node
                    stationaryDistribution[i] += nei.second.second;
                }
            }
        }
    }
    // normalize each stationaryDistribution entry by the total edge weights in the state transition matrix
    double totalEdgeWeights = 0.0;
    for (const auto &node : nodes)
    {
        for (const auto &nei : node->neighbors)
        {
            totalEdgeWeights += nei.second.second;
        }
    }
    for (auto &entry : stationaryDistribution)
    {
        entry /= totalEdgeWeights;
    }
    // second, calculate the average entropy of each node (the entropy rate)
    double entropyRate = 0.0;
    for (unsigned int i = 0; i < stationaryDistribution.size(); i++)
    {
        auto it = nodes.begin();
        advance(it, i);
        for (const auto &nei : (*it)->neighbors)
        {
            entropyRate -= stationaryDistribution[i] * nei.second.second * log2(nei.second.second);
        }
    }
    return entropyRate;
}

double TotalEntropy(std::set<GraphNode *, p_GNCompare> &nodes)
{
    double accumulatedEntropy = 0.0;
    for (const auto &node : nodes)
    {
        for (const auto &nei : node->neighbors)
        {
            accumulatedEntropy -= nei.second.second * log2(nei.second.second);
        }
    }
    return accumulatedEntropy;
}

int main(int argc, char *argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    auto blockCallers = ReadBlockInfo(BlockInfoFilename);
    auto blockLabels = ReadBlockLabels(BlockInfoFilename);
    auto SourceBitcode = ReadBitcode(BitcodeFileName);
    if (SourceBitcode == nullptr)
    {
        return EXIT_FAILURE;
    }
    // Annotate its bitcodes and values
    //CleanModule(SourceBitcode.get());
    Format(SourceBitcode.get());
    // construct its callgraph
    map<int64_t, BasicBlock *> IDToBlock;
    map<int64_t, Value *> IDToValue;
    InitializeIDMaps(SourceBitcode.get(), IDToBlock, IDToValue);

    // Construct bitcode CallGraph
    map<BasicBlock *, Function *> BlockToFPtr;
    auto CG = getCallGraph(SourceBitcode.get(), blockCallers, BlockToFPtr, IDToBlock);

    // Set of nodes that constitute the entire graph
    set<GraphNode *, p_GNCompare> nodes;
    // maps each block ID to its frequency count (used for performance intrinsics calculations later, must happen before transforms because GraphNodes are 1:1 with blocks)
    map<int64_t, uint64_t> blockFrequencies;

    try
    {
        auto err = ReadBIN(nodes, InputFilename);
        if (err)
        {
            spdlog::critical("Failed to read input profile file!");
            return EXIT_FAILURE;
        }
        if (nodes.empty())
        {
            return EXIT_FAILURE;
        }
        // accumulate block frequencies
        for (const auto &block : nodes)
        {
            // we sum along the columns (the probabilities of going to the current node), so we use the edge weight coming from each predecessor to this node
            for (const auto &pred : block->predecessors)
            {
                auto predecessor = nodes.find(pred);
                // find the edge of the predecessor that goes to the current node
                for (const auto &nei : (*predecessor)->neighbors)
                {
                    if (nei.first == block->NID)
                    {
                        // retrieve the edge probability and accumulate it to this node
                        blockFrequencies[(int64_t)nei.first] += nei.second.first;
                    }
                }
            }
        }
    }
    catch (AtlasException &e)
    {
        spdlog::critical(e.what());
        return EXIT_FAILURE;
    }

#ifdef DEBUG
    spdlog::info("Input control flow graph:");
    PrintGraph(nodes);
#endif

    // transform graph in an iterative manner until the size of the graph doesn't change
    size_t graphSize = nodes.size();
    auto startEntropy = EntropyCalculation(nodes);
    auto startTotalEntropy = TotalEntropy(nodes);
    auto startNodes = nodes.size();
    uint64_t startEdges = 0;
    for (const auto &node : nodes)
    {
        startEdges += node->neighbors.size();
    }
    while (true)
    {
        try
        {
            // combine all trivial node merges
            TrivialTransforms(nodes, IDToBlock);
            // Next transform, find conditional branches and turn them into select statements
            // In other words, find subgraphs of nodes that have a common entrance and exit, flow from one end to the other, and combine them into a single node
            BranchToSelectTransforms(nodes);
            // Finally, transform the graph bottlenecks to avoid multiple entrance/multiple exit kernels
            FanInFanOutTransform(nodes);
            if (graphSize == nodes.size())
            {
                break;
            }
            graphSize = nodes.size();
        }
        catch (AtlasException &e)
        {
            spdlog::critical(e.what());
            return EXIT_FAILURE;
        }
    }

#ifdef DEBUG
    spdlog::info("Transformed Graph:");
    PrintGraph(nodes);
#endif

    auto endEntropy = EntropyCalculation(nodes);
    auto endTotalEntropy = TotalEntropy(nodes);
    auto endNodes = nodes.size();
    uint64_t endEdges = 0;
    for (const auto &node : nodes)
    {
        endEdges += node->neighbors.size();
    }

    // find minimum cycles
    bool done = false;
    // master set of kernels, holds all valid kernels parsed from the CFG
    // each kernel in here is represented in the call graph by a virtual kernel node
    set<Kernel *, KCompare> kernels;
    while (!done)
    {
        // holds kernels parsed from this iteration
        set<Kernel *, KCompare> newKernels;
        // first, find min paths in the graph
        for (const auto &node : nodes)
        {
            auto nodeIDs = Dijkstras(nodes, node->NID, node->NID);
            if (!nodeIDs.empty())
            {
                try
                {
                    // check for cycles within the kernel, if it has more than 1 cycle this kernel will be thrown out
                    set<set<uint64_t>> cycles;
                    cycles.insert(set<uint64_t>(nodeIDs.begin(), nodeIDs.end()));
                    set<GraphNode *, p_GNCompare> kernelGraph;
                    for (auto &id : nodeIDs)
                    {
                        kernelGraph.insert(*(nodes.find(id)));
                    }
                    for (const auto &node : kernelGraph)
                    {
                        auto newCycle = Dijkstras(kernelGraph, node->NID, node->NID);
                        cycles.insert(set<uint64_t>(newCycle.begin(), newCycle.end()));
                    }
                    if (cycles.size() > 1)
                    {
                        continue;
                    }
                    auto newKernel = new Kernel();
                    for (const auto &id : nodeIDs)
                    {
                        newKernel->nodes.insert(**(nodes.find(id)));
                    }
                    // check for overlap with kernels from this iteration
                    bool overlap = false;
                    // set of kernels that are being kicked out of the newKernels set
                    set<Kernel *, KCompare> toRemove;
                    for (const auto &kern : newKernels)
                    {
                        auto shared = kern->Compare(*newKernel);
                        if (shared.size() == kern->getBlocks().size())
                        {
                            // if perfect overlap, this kernel has already been found
                            overlap = true;
                        }
                        if (!shared.empty())
                        {
                            // we have an overlap with another kernel, there are two cases that we want to look at
                            // 1.) shared function
                            // 2.) kernel hierarchy

                            // First, check for shared function
                            // two overlapping functions need to satisfy the following condition in order for a shared function to be identified
                            // 1.) All shared blocks are children of functions that are called within each kernel
                            // first condition, all shared blocks are children of called functions within the kernel
                            // set of all functions called in each of the two kernels
                            set<Function *> calledFunctions;
                            for (const auto &caller : blockCallers)
                            {
                                // only check newKernel because it is the kernel in question
                                if (newKernel->getBlocks().find(caller.first) != newKernel->getBlocks().end())
                                {
                                    calledFunctions.insert(IDToBlock[caller.second]->getParent());
                                }
                            }
                            for (const auto &block : shared)
                            {
                                if (calledFunctions.find(IDToBlock[block]->getParent()) == calledFunctions.end())
                                {
                                    // this shared block had a parent that was not part of the called functions within the kernel, so mark these two kernels as overlapping
                                    overlap = true;
                                }
                            }

                            // Second, compare probability of exit. We will keep the loop that is less probable to exit
                            if (newKernel->ExitProbability() < kern->ExitProbability())
                            {
                                // we keep the new kernel, add the compare kernel to the remove list
                                toRemove.insert(kern);
                            }
                            else
                            {
                                overlap = true;
                            }
                        }
                    }
                    if (!overlap)
                    {
                        for (const auto &node : newKernel->nodes)
                        {
                            // we have to get exactly the node from the graph in order to support polymorphism
                            auto potentialVKN = nodes.find(node.NID);
                            if (potentialVKN != nodes.end())
                            {
                                if (auto VKN = dynamic_cast<VKNode *>(*potentialVKN))
                                {
                                    newKernel->childKernels.insert(VKN->kernel->KID);
                                }
                            }
                            else
                            {
                                throw AtlasException("Could not find kernel node in the graph!");
                            }
                        }
                        newKernels.insert(newKernel);
                    }
                    else
                    {
                        delete newKernel;
                    }
                    for (auto remove : toRemove)
                    {
                        newKernels.erase(remove);
                        delete remove;
                    }
                }
                catch (AtlasException &e)
                {
                    spdlog::error(e.what());
                }
            }
        }
        // finally, check to see if we found new kernels, and if we didn't we're done
        auto newPointers = VirtualizeKernels(newKernels, nodes);
        if (!newPointers.empty())
        {
            for (const auto &p : newPointers)
            {
                kernels.insert(p);
            }
            done = false;
        }
        else
        {
            // we have no new virtualized kernels, which is as good as having no new kernels at all, so we're done
            done = true;
        }
    }

#ifdef DEBUG
    spdlog::info("Resulting DAG:");
    PrintGraph(nodes);
#endif

    // majority label vote for kernels
    for (const auto &kernel : kernels)
    {
        map<string, int64_t> labelVotes;
        labelVotes[""] = 0;
        for (const auto &node : kernel->nodes)
        {
            for (const auto &block : node.blocks)
            {
                auto infoEntry = blockLabels.find(block);
                if (infoEntry != blockLabels.end())
                {
                    for (const auto &label : (*infoEntry).second)
                    {
                        if (labelVotes.find(label.first) == labelVotes.end())
                        {
                            labelVotes[label.first] = label.second;
                        }
                        else
                        {
                            labelVotes[label.first] += label.second;
                        }
                    }
                }
                else
                {
                    // no entry for this block, so votes for no label
                    labelVotes[""]++;
                }
            }
        }
        string maxVoteLabel;
        int64_t maxVoteCount = 0;
        for (const auto &label : labelVotes)
        {
            if (label.second > maxVoteCount)
            {
                maxVoteLabel = label.first;
                maxVoteCount = label.second;
            }
        }
        kernel->Label = maxVoteLabel;
    }

    // write kernel file
    json outputJson;
    // valid blocks and block callers sections provide tik with necessary info about the CFG
    outputJson["ValidBlocks"] = std::vector<int64_t>();
    for (const auto &id : IDToBlock)
    {
        outputJson["ValidBlocks"].push_back(id.first);
    }
    for (const auto &bid : blockCallers)
    {
        outputJson["BlockCallers"][to_string(bid.first)] = bid.second;
    }
    // Entropy information
    outputJson["Entropy"] = map<string, map<string, uint64_t>>();
    outputJson["Entropy"]["Start"]["Entropy Rate"] = startEntropy;
    outputJson["Entropy"]["Start"]["Total Entropy"] = startTotalEntropy;
    outputJson["Entropy"]["Start"]["Nodes"] = startNodes;
    outputJson["Entropy"]["Start"]["Edges"] = startEdges;
    outputJson["Entropy"]["End"]["Entropy Rate"] = endEntropy;
    outputJson["Entropy"]["End"]["Total Entropy"] = endTotalEntropy;
    outputJson["Entropy"]["End"]["Nodes"] = endNodes;
    outputJson["Entropy"]["End"]["Edges"] = endEdges;

    // sequential ID for each kernel and a map from KID to sequential ID
    uint32_t id = 0;
    map<uint32_t, uint32_t> SIDMap;
    // average nodes per kernel
    float totalNodes = 0.0;
    // average blocks per kernel
    float totalBlocks = 0.0;
    for (const auto &kernel : kernels)
    {
        totalNodes += (float)kernel->nodes.size();
        totalBlocks += (float)kernel->getBlocks().size();
        for (const auto &n : kernel->nodes)
        {
            outputJson["Kernels"][to_string(id)]["Nodes"].push_back(n.NID);
        }
        for (const auto &k : kernel->getBlocks())
        {
            outputJson["Kernels"][to_string(id)]["Blocks"].push_back(k);
        }
        outputJson["Kernels"][to_string(id)]["Labels"] = std::vector<string>();
        outputJson["Kernels"][to_string(id)]["Labels"].push_back(kernel->Label);
        SIDMap[kernel->KID] = id;
        id++;
    }
    // now assign hierarchy to each kernel
    for (const auto &kern : kernels)
    {
        outputJson["Kernels"][to_string(SIDMap[kern->KID])]["Children"] = vector<uint32_t>();
        outputJson["Kernels"][to_string(SIDMap[kern->KID])]["Parents"] = vector<uint32_t>();
    }
    for (const auto &kern : kernels)
    {
        // fill in parent category for children while we're filling in the children
        for (const auto &child : kern->childKernels)
        {
            outputJson["Kernels"][to_string(SIDMap[kern->KID])]["Children"].push_back(SIDMap[child]);
            outputJson["Kernels"][to_string(SIDMap[child])]["Parents"].push_back(SIDMap[kern->KID]);
        }
    }
    if (!kernels.empty())
    {
        outputJson["Average Kernel Size (Nodes)"] = float(totalNodes / (float)kernels.size());
        outputJson["Average Kernel Size (Blocks)"] = float(totalBlocks / (float)kernels.size());
    }
    else
    {
        outputJson["Average Kernel Size (Nodes)"] = 0.0;
        outputJson["Average Kernel Size (Blocks)"] = 0.0;
    }

    // performance intrinsics
    map<string, set<int64_t>> kernelBlockSets;
    for (const auto &kernel : outputJson["Kernels"].items())
    {
        if (outputJson["Kernels"].find(kernel.key()) != outputJson["Kernels"].end())
        {
            if (outputJson["Kernels"][kernel.key()].find("Blocks") != outputJson["Kernels"][kernel.key()].end())
            {
                auto blockSet = outputJson["Kernels"][kernel.key()]["Blocks"].get<set<int64_t>>();
                kernelBlockSets[kernel.key()] = blockSet;
            }
        }
    }

    auto prof = ProfileKernels(kernelBlockSets, SourceBitcode.get(), blockFrequencies);
    for (const auto &kernelID : prof)
    {
        outputJson["Kernels"][kernelID.first]["Performance Intrinsics"] = kernelID.second;
    }
    ofstream oStream(OutputFilename);
    oStream << setw(4) << outputJson;
    oStream.close();
    if (!DotFile.empty())
    {
        ofstream dStream(DotFile);
        auto graph = GenerateDot(nodes, kernels);
        dStream << graph << "\n";
        dStream.close();
    }

    // free kernel set and nodes

    /*for (const auto node : nodes)
    {
        RemoveNode(nodes, node);
    }*/
    for (const auto &kern : kernels)
    {
        delete kern;
    }
    return 0;
}

/* Simple Type 2 case 1 transform example
    GraphNode zeroth = GraphNode(0);
    zeroth.blocks[0] = 0;
    GraphNode first = GraphNode(1);
    first.blocks[1] = 1;
    GraphNode second = GraphNode(2);
    second.blocks[2] = 2;
    GraphNode third = GraphNode(3);
    third.blocks[3] = 3;
    GraphNode fourth = GraphNode(4);
    fourth.blocks[4] = 4;
    GraphNode fifth = GraphNode(5);
    fifth.blocks[5] = 5;

    zeroth.neighbors[1] = pair(1, 1);
    first.predecessors.insert(0);
    first.neighbors[2] = pair(1, 1);
    second.predecessors.insert(1);
    second.neighbors[3] = pair(1, 0.5);
    second.neighbors[4] = pair(1, 0.5);
    third.predecessors.insert(2);
    third.neighbors[4] = pair(1, 1);
    fourth.predecessors.insert(2);
    fourth.predecessors.insert(3);
    fourth.neighbors[5] = pair(1, 1);
    fifth.predecessors.insert(4);

    nodes.insert(zeroth);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);
    nodes.insert(fourth);
    nodes.insert(fifth);
    */

/* test bench 1 - exercises simple type 2 transform case 2 (2 midnodes)
    GraphNode zeroth = GraphNode(0);
    zeroth.blocks[0] = 0;
    GraphNode first = GraphNode(1);
    first.blocks[1] = 1;
    GraphNode second = GraphNode(2);
    second.blocks[2] = 2;
    GraphNode third = GraphNode(3);
    third.blocks[3] = 3;
    GraphNode fourth = GraphNode(4);
    fourth.blocks[4] = 4;
    GraphNode fifth = GraphNode(5);
    fifth.blocks[5] = 5;
    GraphNode sixth = GraphNode(6);
    sixth.blocks[6] = 6;

    zeroth.neighbors[1] = pair(1, 1);
    first.predecessors.insert(0);
    first.neighbors[2] = pair(1, 1);
    second.predecessors.insert(1);
    second.neighbors[3] = pair(1, 0.5);
    second.neighbors[4] = pair(1, 0.5);
    third.predecessors.insert(2);
    third.neighbors[5] = pair(1, 1);
    fourth.predecessors.insert(2);
    fourth.neighbors[5] = pair(1, 1);
    fifth.predecessors.insert(3);
    fifth.predecessors.insert(4);
    fifth.neighbors[6] = pair(1, 1);
    sixth.predecessors.insert(5);

    nodes.insert(zeroth);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);
    nodes.insert(fourth);
    nodes.insert(fifth);
    nodes.insert(sixth);
    */

/* Test bench 2 - type 2 transform case 1 with 3 midnodes
    GraphNode zeroth = GraphNode(0);
    zeroth.blocks[0] = 0;
    GraphNode first = GraphNode(1);
    first.blocks[1] = 1;
    GraphNode second = GraphNode(2);
    second.blocks[2] = 2;
    GraphNode third = GraphNode(3);
    third.blocks[3] = 3;
    GraphNode fourth = GraphNode(4);
    fourth.blocks[4] = 4;
    GraphNode fifth = GraphNode(5);
    fifth.blocks[5] = 5;
    GraphNode sixth = GraphNode(6);
    sixth.blocks[6] = 6;
    GraphNode seventh = GraphNode(7);
    seventh.blocks[7] = 7;

    zeroth.neighbors[1] = pair(1, 1);
    first.predecessors.insert(0);
    first.neighbors[2] = pair(1, 1);
    second.predecessors.insert(1);
    second.neighbors[3] = pair(1, 0.25);
    second.neighbors[4] = pair(1, 0.25);
    second.neighbors[5] = pair(1, 0.25);
    second.neighbors[6] = pair(1, 0.25);
    third.predecessors.insert(2);
    third.neighbors[6] = pair(1, 1);
    fourth.predecessors.insert(2);
    fourth.neighbors[6] = pair(1, 1);
    fifth.predecessors.insert(2);
    fifth.neighbors[6] = pair(1, 1);
    sixth.predecessors.insert(2);
    sixth.predecessors.insert(3);
    sixth.predecessors.insert(4);
    sixth.predecessors.insert(5);
    sixth.neighbors[7] = pair(1, 1);
    seventh.predecessors.insert(6);

    nodes.insert(zeroth);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);
    nodes.insert(fourth);
    nodes.insert(fifth);
    nodes.insert(sixth);
    nodes.insert(seventh);
    */

/* Test bench 3 - type 2 transform case 2 with 3 midnodes
    GraphNode zeroth = GraphNode(0);
    zeroth.blocks[0] = 0;
    GraphNode first = GraphNode(1);
    first.blocks[1] = 1;
    GraphNode second = GraphNode(2);
    second.blocks[2] = 2;
    GraphNode third = GraphNode(3);
    third.blocks[3] = 3;
    GraphNode fourth = GraphNode(4);
    fourth.blocks[4] = 4;
    GraphNode fifth = GraphNode(5);
    fifth.blocks[5] = 5;
    GraphNode sixth = GraphNode(6);
    sixth.blocks[6] = 6;
    GraphNode seventh = GraphNode(7);
    seventh.blocks[7] = 7;

    zeroth.neighbors[1] = pair(1, 1);
    first.predecessors.insert(0);
    first.neighbors[2] = pair(1, 1);
    second.predecessors.insert(1);
    second.neighbors[3] = pair(1, 0.333);
    second.neighbors[4] = pair(1, 0.333);
    second.neighbors[5] = pair(1, 0.333);
    third.predecessors.insert(2);
    third.neighbors[6] = pair(1, 1);
    fourth.predecessors.insert(2);
    fourth.neighbors[6] = pair(1, 1);
    fifth.predecessors.insert(2);
    fifth.neighbors[6] = pair(1, 1);
    sixth.predecessors.insert(3);
    sixth.predecessors.insert(4);
    sixth.predecessors.insert(5);
    sixth.neighbors[7] = pair(1, 1);
    seventh.predecessors.insert(6);

    nodes.insert(zeroth);
    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);
    nodes.insert(fourth);
    nodes.insert(fifth);
    nodes.insert(sixth);
    nodes.insert(seventh);
    */