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
cl::opt<string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("kernel filename"), cl::Required);

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
    CFG.erase(removeNode);
    delete removeNode;
}

void RemoveNode(std::set<GraphNode *, p_GNCompare> &CFG, const GraphNode &removeNode)
{
    // fourth, free
    CFG.erase(CFG.find(removeNode.NID));
}

void ReadBIN(std::set<GraphNode *, p_GNCompare> &nodes, const std::string &filename, bool print = false)
{
    std::fstream inputFile;
    inputFile.open(filename, std::ios::in | std::ios::binary);
    if (!inputFile.good())
    {
        spdlog::critical("Could not open input file " + filename + " for reading.");
        return;
    }
    while (inputFile.peek() != EOF)
    {
        // New block description: BBID,#ofNeighbors (16 bytes per neighbor)
        uint64_t key;
        inputFile.readsome((char *)&key, sizeof(uint64_t));
        GraphNode currentNode;
        if (nodes.find(key) == nodes.end())
        {
            currentNode = GraphNode(key);
            // when reading the trace file, NID and blockID are 1to1
            currentNode.blocks[(int64_t)key] = (int64_t)key;
        }
        else
        {
            spdlog::error("Found a BBID that already existed in the graph!");
        }
        // the instance count of the edge
        uint64_t count;
        // for summing the total count of the neighbors
        uint64_t sum = 0;
        inputFile.readsome((char *)&count, sizeof(uint64_t));
        for (uint64_t i = 0; i < count; i++)
        {
            uint64_t k2;
            inputFile.readsome((char *)&k2, sizeof(uint64_t));
            uint64_t val;
            inputFile.readsome((char *)&val, sizeof(uint64_t));
            if (val > 0)
            {
                sum += val;
                currentNode.neighbors[k2] = std::pair(val, 0.0);
            }
        }
        for (auto &key : currentNode.neighbors)
        {
            key.second.second = (double)key.second.first / (double)sum;
        }
        AddNode(nodes, currentNode);
    }
    inputFile.close();

    // now fill in all the predecessor nodes
    for (auto &node : nodes)
    {
        for (const auto &neighbor : node->neighbors)
        {
            auto successorNode = nodes.find(neighbor.first);
            if (successorNode != nodes.end())
            {
                (*successorNode)->predecessors.insert(node->NID);
            }
            // the trace doesn't include the terminating block of the program (because it has no edges leading from it)
            // But this creates a problem when defining kernel exits, so look for the node who has a neighbor that is not in the set already and add that neighbor (with correct predecessor)
            else
            {
                // we likely found the terminating block, so add the block and assign the current node to be its predecessor
                auto programTerminator = GraphNode(neighbor.first);
                programTerminator.blocks[(int64_t)programTerminator.NID] = (int64_t)programTerminator.NID;
                programTerminator.predecessors.insert(node->NID);
                AddNode(nodes, programTerminator);
            }
        }
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
                            spdlog::error("Found a node in the graph whose ID did not map to a basic block pointer in the ID map!");
                            break;
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
                    break;
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
            std::set<uint64_t> midNodeTargets;
            for (const auto &midNode : entrance->neighbors)
            {
                midNodes.insert(midNode.first);
                if (nodes.find(midNode.first) != nodes.end())
                {
                    for (const auto &neighbor : (*nodes.find(midNode.first))->neighbors)
                    {
                        midNodeTargets.insert(neighbor.first);
                    }
                }
                else
                {
                    // ensures that all our midnodes can be found
                    break;
                }
            }
            if (midNodeTargets.size() == 1) // corner case where the exit of the subgraph has no successors (it is the last node to execute in the program). In this case we have to check if the entrance is a predecessor of the lone midNodeTarget
            {
                auto cornerCase = nodes.find(*midNodeTargets.begin());
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
                        potentialExit = nodes.find(*midNodeTargets.begin());
                    }
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
                    badCondition = true;
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
                    badCondition = true;
                }
            }
            if (badCondition)
            {
                break;
            }
            // 4.) potentialExit can only have the midnodes as predecessors
            tmpMids = midNodes;
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
            }
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
                // case 2: all entrance neighbors have a common successor
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
                // case 1: all entrance neighbors have a common successor, and the entrance is a predecessor of that successor
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
                for (auto &n : (*potentialExit)->predecessors)
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
            }
            // merge midNodes and potentialExit blocks in order
            for (auto n : midNodes)
            {
                auto midNode = nodes.find(n);
                if (midNode != nodes.end())
                {
                    entrance->addBlocks((*midNode)->blocks);
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
                                spdlog::error("Found a neighbor that does not exist within the graph!");
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
                        spdlog::error("Found a neighbor that does not exist in the graph!");
                        continue;
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
        // gather entrance and exit nodes
        auto kernelEntrances = kernel->getEntrances();
        auto kernelExits = kernel->getExits();
        try
        {
            if ((kernelEntrances.size() == 1) && (kernelExits.size() == 1))
            {
                // change the neighbors of the entrance node to the exit node (because the exit nodes are the first blocks outside the kernel exit edges)
                auto entranceNode = new VKNode(*kernelEntrances.begin(), kernel);
                // remove each node in the kernel that are possibly in the node predecessors
                for (const auto &node : kernel->nodes)
                {
                    entranceNode->predecessors.erase(node.NID);
                }
                auto exitNode = *(kernelExits.begin());
                // investigate the neighbors
                // the only edges leading from the virtual kernel node should be edges out of the kernel
                // we are given the nodes within the kernel that have neighbors outside the kernel, so here we figure out which exit edges exist
                // we leave the probabilities the same as the original nodes (this means we will have outgoing edges whose probabilities do not sum to 1, because edges that go back in the kernel are omitted)
                entranceNode->neighbors.clear();
                for (const auto &en : exitNode.neighbors)
                {
                    // nodes within the kernel will not be in the neighbors of the virtual kernel
                    if (kernel->nodes.find(en.first) == kernel->nodes.end())
                    {
                        if (kernel->nodes.find(en.first) == kernel->nodes.end())
                        {
                            auto exitNode = nodes.find(en.first);
                            if (exitNode != nodes.end())
                            {
                                entranceNode->neighbors[en.first] = en.second;
                            }
                        }
                    }
                }
                // now we have to figure out if the
                // for each neighbor of the kernel exit, change its predecessor to the kernel entrance node
                for (const auto &neighID : exitNode.neighbors)
                {
                    auto neighbor = *(nodes.find(neighID.first));
                    neighbor->predecessors.erase(exitNode.NID);
                    neighbor->predecessors.insert(entranceNode->NID);
                }
                // remove all nodes within the kernel except the entrance node
                auto toRemove = kernel->nodes;
                for (const auto &node : kernel->getEntrances())
                {
                    toRemove.erase(node);
                }
                for (const auto &node : toRemove)
                {
                    RemoveNode(nodes, node);
                }
                // finally replace the old entrance node with the new VKNode
                RemoveNode(nodes, *(kernelEntrances.begin()));
                nodes.insert(entranceNode);
                kernel->kernelNode = entranceNode;
                newPointers.push_back(kernel);
            }
            else
            {
                throw AtlasException("Kernel ID " + to_string(kernel->KID) + " has " + to_string(kernelEntrances.size()) + " entrances and " + to_string(kernelExits.size()) + " exits!");
            }
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
    CleanModule(SourceBitcode.get());
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

    ReadBIN(nodes, InputFilename);
    if (nodes.empty())
    {
        return EXIT_FAILURE;
    }

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
                            // keep the existing kernel, throw out the new one
                            overlap = true;
                        }
                    }
                }
                if (!overlap)
                {
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
                auto infoEntry = blockLabels.find(block.first);
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

    // sequential ID for each kernel
    int id = 0;
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
        id++;
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
    ofstream oStream(OutputFilename);
    oStream << setw(4) << outputJson;
    oStream.close();
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