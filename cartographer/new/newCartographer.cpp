#include "newCartographer.h"
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

vector<uint64_t> Dijkstras(set<GraphNode, GNCompare> &nodes, uint64_t source, uint64_t sink)
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

    spdlog::trace("Loading bin file: {0}", InputFilename);
    set<GraphNode, GNCompare> nodes;
    fstream inputFile;
    inputFile.open(InputFilename, ios::in | ios::binary);
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
    for (const auto &node : nodes)
    {
        spdlog::info("Examining node " + to_string(node.NID));
        for (const auto &neighbor : node.neighbors)
        {
            spdlog::info("Neighbor " + to_string(neighbor.first) + " has instance count " + to_string(neighbor.second.first) + " and probability " + to_string(neighbor.second.second));
        }
        cout << endl;
        //spdlog::info("Node " + to_string(node.NID) + " has " + to_string(node.neighbors.size()) + " neighbors.");
    }
    // first index is the node to remove, second index is the nodeID in which the toRemove node has been merged
    set<pair<uint64_t, uint64_t>, PairComp<uint64_t>> toRemove;
    // combine all trivial edges
    vector<GraphNode> tmpNodes(nodes.begin(), nodes.end());
    for (auto &node : tmpNodes)
    {
        if (toRemove.find(node.NID) != toRemove.end())
        {
            continue;
        }
        // if this node only has 1 edge with certain probability
        if ((node.neighbors.size() == 1) && (node.neighbors.begin()->second.second > 0.9999))
        {
            // there is trivial merge opportunity here
            // we always combine to the source node because we can't go against the edge directions
            bool done = false;
            while (!done)
            {
                auto neighbor = tmpNodes.end();
                for (auto n = tmpNodes.begin(); n != tmpNodes.end(); n++)
                {
                    if (n->NID == node.neighbors.begin()->first)
                    {
                        neighbor = n;
                    }
                }
                if (neighbor != tmpNodes.end() && toRemove.find(neighbor->NID) == toRemove.end())
                {

                    // 1.) add sink node's blocks to source node
                    // a.) first, find the block that maps to itself and change its value to the new neighbor. This updates the sequential chain map to have the new merged node as the head node in the chain
                    for (auto &b : node.blocks)
                    {
                        if (b.first == b.second)
                        {
                            b.second = neighbor->NID;
                            break; // should only have one match
                        }
                    }
                    // b.) make the sink node the head of the source node block map by mapping it to itself
                    node.blocks[neighbor->NID] = neighbor->NID;
                    for (const auto &b : neighbor->blocks)
                    {
                        node.blocks[b.first] = b.second;
                    }
                    // 2.) put the sink node in the remove set
                    // a.) add the node and its new mapping
                    toRemove.insert(pair(neighbor->NID, node.NID));
                    // b.) look for any other entries that map to the block that has now been merged
                    set<pair<uint64_t, uint64_t>, PairComp<uint64_t>> toChange;
                    for (auto &r : toRemove)
                    {
                        if (r.second == neighbor->NID)
                        {
                            // make a replacement entry for r.first (the ID of a block that must move nodes now)
                            toChange.insert(pair(r.first, node.NID));
                        }
                    }
                    // erase() only looks for c.first, so we can delete the old pair with c, even though c.second is different
                    for (const auto &c : toChange)
                    {
                        toRemove.erase(c);
                        toRemove.insert(c);
                    }
                    // 3.) make the source node neighbors the sink node neighbors
                    node.neighbors.clear();
                    for (const auto &k : neighbor->neighbors)
                    {
                        node.neighbors[k.first] = k.second;
                    }
                    // 4.) check if this is the end of a sequential chain of nodes, and continue if it's not
                    if ((node.neighbors.size() == 1) && (node.neighbors.begin()->second.second > 0.9999))
                    {
                        done = false;
                    }
                    else
                    {
                        done = true;
                    }
                }
                else
                {
                    // here we have to make a recording of a neighbor change to preserve the neighbor edge back to a possibly block-merged node
                    node.neighbors[toRemove.find(neighbor->NID)->second] = node.neighbors[toRemove.find(neighbor->NID)->first];
                    node.neighbors.erase(toRemove.find(neighbor->NID)->first);
                    done = true;
                }
            }
        }
    }
    nodes.clear();
    for (auto &node : tmpNodes)
    {
        if (toRemove.find(node.NID) == toRemove.end())
        {
            // if any of our neighbors have been merged we need to fix them
            vector<uint64_t> badNeighbors;
            for (auto &neighbor : node.neighbors)
            {
                if (toRemove.find(neighbor.first) != toRemove.end())
                {
                    // we have to find which node our neighbor was merged into
                    for (const auto &newNeighbor : tmpNodes)
                    {
                        if (newNeighbor.neighbors.find(neighbor.first) != newNeighbor.neighbors.end())
                        {
                            node.neighbors[newNeighbor.NID] = neighbor.second;
                        }
                    }
                    // and remove the old one
                    badNeighbors.push_back(neighbor.first);
                }
            }
            for (auto &e : badNeighbors)
            {
                node.neighbors.erase(e);
            }
            nodes.insert(node);
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
        auto blocksMap = kernel.getBlocks(false);
        for (const auto &k : kernel.getBlocks(false))
        {
            outputJson["Kernels"][to_string(id)]["Blocks"].push_back(k.first);
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

/* simple dijkstra example
    GraphNode first = GraphNode(0);
    first.blocks.insert(0);
    GraphNode second = GraphNode(1);
    second.blocks.insert(1);
    GraphNode third = GraphNode(2);
    third.blocks.insert(2);
    GraphNode fourth = GraphNode(3);
    fourth.blocks.insert(3);
    GraphNode fifth = GraphNode(4);
    fifth.blocks.insert(4);
    GraphNode sixth = GraphNode(5);
    sixth.blocks.insert(5);

    first.neighbors[1] = pair(9, 0.9);
    second.neighbors[2] = pair(1, 0.1);
    second.neighbors[3] = pair(9, 0.9);
    third.neighbors[0] = pair(1, 1);
    fourth.neighbors[4] = pair(9, 1);
    fifth.neighbors[5] = pair(9, 1);
    sixth.neighbors[0] = pair(9, 1);

    nodes.insert(first);
    nodes.insert(second);
    nodes.insert(third);
    nodes.insert(fourth);
    nodes.insert(fifth);
    nodes.insert(sixth);
    */