#include "newCartographer.h"
#include "AtlasUtil/Exceptions.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <llvm/Support/CommandLine.h>
#include <nlohmann/json.hpp>
#include <queue>
#include <spdlog/spdlog.h>

using namespace llvm;
using namespace std;
using json = nlohmann::json;

cl::opt<string> InputFilename("i", cl::desc("Specify bin file"), cl::value_desc(".bin filename"), cl::Required);
cl::opt<string> BlockInfoFilename("b", cl::desc("Specify BlockInfo.json file"), cl::value_desc(".json filename"), cl::Required);
cl::opt<string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("kernel filename"), cl::Required);

uint64_t GraphNode::nextNID = 0;
uint32_t Kernel::nextKID = 0;

void ReadBIN(set<GraphNode, GNCompare> &nodes, const string &filename)
{
    // set that holds the first iteration of the graph
    fstream inputFile;
    inputFile.open(filename, ios::in | ios::binary);
    while (inputFile.peek() != EOF)
    {
        // New block description: BBID,#ofNeighbors (16 bytes per neighbor)
        uint64_t key;
        inputFile.readsome((char *)&key, sizeof(uint64_t));
        GraphNode currentNode;
        if (nodes.find(key) == nodes.end())
        {
            currentNode = GraphNode(key);
            // right now, NID and blocks are 1to1
            currentNode.blocks[key] = key;
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
                currentNode.neighbors[k2] = pair(val, 0.0);
            }
        }
        for (auto &key : currentNode.neighbors)
        {
            key.second.second = (double)key.second.first / (double)sum;
        }
        nodes.insert(currentNode);
    }
    inputFile.close();

    // now fill in all the predecessor nodes
    for (auto &node : nodes)
    {
        for (const auto &neighbor : node.neighbors)
        {
            auto successorNode = nodes.find(neighbor.first);
            if (successorNode != nodes.end())
            {
                GraphNode newNode = *successorNode;
                newNode.predecessors.insert(node.NID);
                nodes.erase(*successorNode);
                nodes.insert(newNode);
            }
        }
    }

    for (const auto &node : nodes)
    {
        spdlog::info("Examining node " + to_string(node.NID));
        string preds;
        for (auto pred : node.predecessors)
        {
            preds += to_string(pred);
            if (pred != *prev(node.predecessors.end()))
            {
                preds += ",";
            }
        }
        spdlog::info("Predecessors: " + preds);
        for (const auto &neighbor : node.neighbors)
        {
            spdlog::info("Neighbor " + to_string(neighbor.first) + " has instance count " + to_string(neighbor.second.first) + " and probability " + to_string(neighbor.second.second));
        }
        cout << endl;
        //spdlog::info("Node " + to_string(node.NID) + " has " + to_string(node.neighbors.size()) + " neighbors.");
    }
}

vector<uint64_t> Dijkstras(const set<GraphNode, GNCompare> &nodes, uint64_t source, uint64_t sink)
{
    // maps a node ID to its dijkstra information
    map<uint64_t, DijkstraNode> DMap;
    for (const auto &node : nodes)
    {
        // initialize each dijkstra node to have infinite distance, itself as its predecessor, and the unvisited nodecolor
        DMap[node.NID] = DijkstraNode(INFINITY, node.NID, std::numeric_limits<uint64_t>::max(), NodeColor::White);
    }
    DMap[source] = DijkstraNode(0, source, std::numeric_limits<uint64_t>::max(), NodeColor::White);
    // priority queue that holds all newly discovered nodes. Minimum paths get priority
    // this deque gets sorted before each iteration, emulating the behavior of a priority queue, which is necessary because std::priority_queue does not support DECREASE_KEY operation
    deque<DijkstraNode> Q;
    Q.push_back(DMap[source]);
    while (!Q.empty())
    {
        // sort the priority queue
        std::sort(Q.begin(), Q.end(), DCompare);
        // for each neighbor of u, calculate the neighbors new distance
        if (nodes.find(Q.front().NID) != nodes.end())
        {
            for (const auto &neighbor : nodes.find(Q.front().NID)->neighbors)
            {
                /*spdlog::info("Priority Q has the following entries in this order:");
                for( const auto& entry : Q )
                {
                    spdlog::info(to_string(entry.NID));
                }*/
                if (neighbor.first == source)
                {
                    // we've found a loop
                    // the DMap distance will be 0 for the source node so we can't do a comparison of distances on the first go-round
                    // if the source doesnt yet have a predecessor then update its stats
                    if (DMap[source].predecessor == std::numeric_limits<uint64_t>::max())
                    {
                        DMap[source].predecessor = Q.front().NID;
                        DMap[source].distance = -log(neighbor.second.second) + DMap[Q.front().NID].distance;
                    }
                }
                if (-log(neighbor.second.second) + Q.front().distance < DMap[neighbor.first].distance)
                {
                    DMap[neighbor.first].predecessor = Q.front().NID;
                    DMap[neighbor.first].distance = -log(neighbor.second.second) + DMap[Q.front().NID].distance;
                    if (DMap[neighbor.first].color == NodeColor::White)
                    {
                        DMap[neighbor.first].color = NodeColor::Grey;
                        Q.push_back(DMap[neighbor.first]);
                    }
                    else if (DMap[neighbor.first].color == NodeColor::Grey)
                    {
                        // we have already seen this neighbor, it must be in the queue. We have to find its queue entry and update it
                        for (auto &node : Q)
                        {
                            if (node.NID == DMap[neighbor.first].NID)
                            {
                                node.predecessor = DMap[neighbor.first].predecessor;
                                node.distance = DMap[neighbor.first].distance;
                            }
                        }
                        std::sort(Q.begin(), Q.end(), DCompare);
                    }
                }
            }
        }
        DMap[Q.front().NID].color = NodeColor::Black;
        Q.pop_front();
    }
    // now construct the min path
    vector<uint64_t> newKernel;
    for (const auto &DN : DMap)
    {
        if (DN.first == sink)
        {
            if (DN.second.predecessor == std::numeric_limits<uint64_t>::max())
            {
                // there was no path found between source and sink
                return newKernel;
            }
            auto prevNode = DN.second.predecessor;
            newKernel.push_back(nodes.find(prevNode)->NID);
            while (prevNode != source)
            {
                prevNode = DMap[prevNode].predecessor;
                newKernel.push_back(nodes.find(prevNode)->NID);
            }
            break;
        }
    }
    return newKernel;
}

int main(int argc, char *argv[])
{
    cl::ParseCommandLineOptions(argc, argv);

    ifstream inputJson;
    nlohmann::json j;
    try
    {
        inputJson.open(BlockInfoFilename);
        inputJson >> j;
        inputJson.close();
    }
    catch (exception &e)
    {
        spdlog::critical("Couldn't open input json file: " + BlockInfoFilename);
        spdlog::critical(e.what());
        return EXIT_FAILURE;
    }
    map<string, vector<int64_t>> blockCallers;
    for (const auto &bbid : j.items())
    {
        blockCallers[bbid.key()] = j[bbid.key()]["BlockCallers"].get<vector<int64_t>>();
    }

    // Set of nodes that constitute the entire graph
    set<GraphNode, GNCompare> nodes;
    ReadBIN(nodes, InputFilename);

    // combine all trivial node merges
    // a trivial node merge must satisfy two conditions
    // 1.) The source node has exactly 1 neighbor with certain probability
    // 2.) The sink node has exactly 1 predecessor (the source node) with certain probability
    // combine all trivial edges
    vector<GraphNode> tmpNodes(nodes.begin(), nodes.end());
    for (auto &node : tmpNodes)
    {
        if (nodes.find(node.NID) == nodes.end())
        {
            // we've already been removed, do nothing
            continue;
        }
        auto currentNode = *nodes.find(node.NID);
        ;
        while (true)
        {
            // first condition, our source node must have 1 certain successor
            if ((currentNode.neighbors.size() == 1) && (currentNode.neighbors.begin()->second.second > 0.9999))
            {
                auto succ = nodes.find(currentNode.neighbors.begin()->first);
                if (succ != nodes.end())
                {
                    // second condition, the sink node must have 1 certain predecessor
                    if ((succ->predecessors.size() == 1) && (succ->predecessors.find(currentNode.NID) != succ->predecessors.end()))
                    {
                        // trivial merge, we merge into the source node
                        auto merged = currentNode;
                        // keep the NID, preds
                        // change the neighbors to the sink neighbors AND update the successors of the sink node to include the merged node in their predecessors
                        merged.neighbors.clear();
                        for (const auto &n : succ->neighbors)
                        {
                            merged.neighbors[n.first] = n.second;
                            auto succ2 = nodes.find(n.first);
                            if (succ2 != nodes.end())
                            {
                                auto newPreds = *succ2;
                                newPreds.predecessors.erase(succ->NID);
                                newPreds.predecessors.insert(merged.NID);
                                nodes.erase(*succ2);
                                nodes.insert(newPreds);
                            }
                        }
                        // add the successor blocks
                        merged.addBlock(succ->NID);

                        // remove stale nodes from the node set
                        nodes.erase(currentNode);
                        nodes.erase(*succ);
                        nodes.insert(merged);

                        currentNode = merged;
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

    // Next transform, find conditional branches and turn them into select statements
    // In other words, condense subgraphs of nodes that have a common entrance and exit, flow from one end to the other, and combine them into a single node
    // Vocabulary
    // entrance - first node that will execute in the target subgraph
    // midnodes - nodes that lie between entrance and exit
    // exit     - last node that will execute in the subgraph
    // Rules
    // 1.) The subgraph must have exactly one entrance and one exit
    // 2.) Exactly one layer of midnodes must exist between entrance and exit. The entrance is allowed to lead directly to the exit
    // 3.) No cycles may exist in the subgraph i.e. Flow can only go from entrance to zero or one midnode to exit
    tmpNodes.clear();
    tmpNodes.insert(tmpNodes.begin(), nodes.begin(), nodes.end());
    for (auto &node : tmpNodes)
    {
        if (nodes.find(node.NID) == nodes.end())
        {
            // we've already been removed, do nothing
            continue;
        }
        auto entrance = *nodes.find(node.NID);
        while (true)
        {
            // flag representing the case we have, if any.
            // false for Case 1, true for Case 2
            bool MergeCase = false;
            // first step, acquire middle nodes
            //
            // second step, check for 1 of 2 configurations for this transform
            // 1.) 0-deep branch->select: entrance can go directly to exit
            // 2.) 1-deep branch->select: entrance cannot go directly to exit
            std::set<uint64_t> midNodeTargets;
            set<uint64_t> midNodes;
            for (const auto &midNode : entrance.neighbors)
            {
                midNodes.insert(midNode.first);
                if (nodes.find(midNode.first) != nodes.end())
                {
                    for (const auto &neighbor : nodes.find(midNode.first)->neighbors)
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
            auto potentialExit = nodes.end();
            if (midNodeTargets.size() == 1) // corner case where the exit of the subgraph has no successors (it is the last node to execute in the program). In this case we have to check if the entrance is a predecessor of the lone midNodeTarget
            {
                auto cornerCase = nodes.find(*midNodeTargets.begin());
                if (cornerCase != nodes.end())
                {
                    if (cornerCase->predecessors.find(entrance.NID) != cornerCase->predecessors.end())
                    {
                        // the entrance can lead directly to the exit
                        MergeCase = false;
                        potentialExit = cornerCase;
                        midNodes.erase(potentialExit->NID);
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
                if (entrance.neighbors.size() > 1)
                {
                    for (auto succ = entrance.neighbors.begin(); succ != entrance.neighbors.end(); succ++)
                    {
                        bool common = true;
                        for (auto neighborID = entrance.neighbors.begin(); neighborID != entrance.neighbors.end(); neighborID++)
                        {
                            if (succ == neighborID)
                            {
                                continue;
                            }
                            auto neighbor = nodes.find(neighborID->first);
                            for (const auto &succ2 : neighbor->neighbors)
                            {
                                if (succ2.first != succ->first)
                                {
                                    common = false;
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
            // in order for either case to be true, five conditions must be checked
            // 1.) the entrance can't have the exit or any midnodes as predecessors
            auto tmpMids = midNodes;
            auto pushed = tmpMids.insert(potentialExit->NID);
            if (!pushed.second)
            {
                // somehow the exit ID is still in the midNodes set
                throw AtlasException("PotentialExit found in the midNodes set!");
            }
            for (auto &pred : entrance.predecessors)
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
                    if ((midNode->predecessors.size() != 1) || (midNode->predecessors.find(entrance.NID) == midNode->predecessors.end()))
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
                    if ((midNode->neighbors.size() != 1) || (midNode->neighbors.find(potentialExit->NID) == midNode->neighbors.end()))
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
            if ((midNodes.size() == potentialExit->predecessors.size()))
            {
                for (auto &pred : potentialExit->predecessors)
                {
                    tmpMids.erase(pred);
                }
                if (!tmpMids.empty())
                {
                    break;
                }
            }
            // 5.) potentialExit can't have the entrance or any midnodes as successors
            tmpMids = midNodes;
            badCondition = false; // flipped if we find a midnode or entrance in potentialExit successors, or we find a bad node
            for (const auto &k : potentialExit->neighbors)
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
            // Now we do case-specific checks
            if (MergeCase)
            {
                // case 2: all entrance neighbors have a common successor
                // 2 conditions must be checked
                // 1.) entrance only has midnodes as successors
                tmpMids = midNodes;
                for (auto &n : entrance.neighbors)
                {
                    tmpMids.erase(n.first);
                }
                if (!tmpMids.empty())
                {
                    break;
                }
                // 2.) potentialExit only has midnodes as predecessors
                tmpMids = midNodes;
                for (auto &n : potentialExit->predecessors)
                {
                    tmpMids.erase(n);
                }
                if (!tmpMids.empty())
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
                tmpMids.insert(potentialExit->NID);
                for (auto &n : entrance.neighbors)
                {
                    tmpMids.erase(n.first);
                }
                if (!tmpMids.empty())
                {
                    break;
                }
                // 2.) potentialExit only has midnodes and entrance as predecessors
                tmpMids = midNodes;
                tmpMids.insert(entrance.NID);
                for (auto &n : potentialExit->predecessors)
                {
                    tmpMids.erase(n);
                }
                if (!tmpMids.empty())
                {
                    break;
                }
            }
            // merge entrance, exit, thirdNode into the entrance
            auto merged = entrance;
            // keep the NID, preds
            // change the neighbors to potentialExit neighbors AND update the successors of potentialExit to include the merged node in their predecessors
            merged.neighbors.clear();
            for (const auto &n : potentialExit->neighbors)
            {
                merged.neighbors[n.first] = n.second;
                auto succ2 = nodes.find(n.first);
                if (succ2 != nodes.end())
                {
                    auto newPreds = *succ2;
                    newPreds.predecessors.erase(potentialExit->NID);
                    newPreds.predecessors.insert(merged.NID);
                    nodes.erase(*succ2);
                    nodes.insert(newPreds);
                }
            }
            // merge midNodes and potentialExit blocks in order
            for (auto n : midNodes)
            {
                auto midNode = nodes.find(n);
                if (midNode != nodes.end())
                {
                    merged.addBlocks(midNode->blocks);
                }
            }
            merged.addBlocks(potentialExit->blocks);

            // remove stale nodes from the node set
            nodes.erase(entrance);
            for (auto n : midNodes)
            {
                auto midNode = nodes.find(n);
                if (midNode != nodes.end())
                {
                    nodes.erase(*midNode);
                }
            }
            nodes.erase(*potentialExit);
            nodes.insert(merged);

            entrance = merged;
            break;
        }
    }

    for (const auto &node : nodes)
    {
        spdlog::info("Examining node " + to_string(node.NID));
        string blocks;
        for (const auto &b : node.blocks)
        {
            blocks += to_string(b.first) + "->" + to_string(b.second) + ",";
        }
        spdlog::info("This node contains blocks: " + blocks);
        for (const auto &neighbor : node.neighbors)
        {
            spdlog::info("Neighbor " + to_string(neighbor.first) + " has instance count " + to_string(neighbor.second.first) + " and probability " + to_string(neighbor.second.second));
        }
        cout << endl;
        //spdlog::info("Node " + to_string(node.NID) + " has " + to_string(node.neighbors.size()) + " neighbors.");
    }

    // find minimum cycles
    bool done = false;
    size_t numKernels = 0;
    set<Kernel, KCompare> kernels;
    while (!done)
    {
        done = true;
        for (const auto &node : nodes)
        {
            auto nodeIDs = Dijkstras(nodes, node.NID, node.NID);
            if (!nodeIDs.empty())
            {
                auto newKernel = Kernel();
                for (const auto &id : nodeIDs)
                {
                    newKernel.nodes.insert(*(nodes.find(id)));
                }
                // compare to other kernels we already have, if any exist
                if (!kernels.empty())
                {
                    bool match = false;
                    for (const auto &kern : kernels)
                    {
                        if (kern.Compare(newKernel) > 0.999)
                        {
                            match = true;
                        }
                    }
                    if (!match)
                    {
                        kernels.insert(newKernel);
                    }
                }
                else
                {
                    kernels.insert(newKernel);
                }
            }
        }
        if (kernels.size() == numKernels)
        {
            done = true;
        }
        else
        {
            done = false;
            numKernels = kernels.size();
        }
    }

    json outputJson;
    outputJson["ValidBlocks"] = std::vector<string>();
    for (const auto &bid : blockCallers)
    {
        outputJson["BlockCallers"][bid.first] = bid.second;
        outputJson["ValidBlocks"].push_back(bid.first);
    }
    int id = 0;
    for (const auto &kernel : kernels)
    {
        for (const auto &k : kernel.getBlocks(false))
        {
            outputJson["Kernels"][to_string(id)]["Blocks"].push_back(k);
        }
        outputJson["Kernels"][to_string(id)]["Labels"] = std::vector<string>();
        outputJson["Kernels"][to_string(id)]["Labels"].push_back("");
        id++;
    }
    ofstream oStream(OutputFilename);
    oStream << setw(4) << outputJson;
    oStream.close();
    return 0;
}

/* Simple Type 2 transform example
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