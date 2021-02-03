#include "newCartographer.h"
#include "AtlasUtil/IO.h"
#include <fstream>
#include <iostream>

using namespace llvm;
using namespace std;

cl::opt<string> InputFilename("i", cl::desc("Specify bin file"), cl::value_desc(".bin filename"), cl::Required);
cl::opt<string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("kernel filename"), cl::Required);

uint64_t GraphNode::nextNID = 0;
uint32_t MinKernel::nextKID = 0;

enum class NodeColor
{
    White,
    Grey,
    Black
};

struct DijkstraNode
{
    DijkstraNode() = default;
    DijkstraNode(double d, uint64_t p, NodeColor c)
    {
        distance = d;
        predecessor = p;
        color = c;
    }
    /// distance between this node and the target source node
    /// since our objective is to find the maximum likelihood path, we need to map probabilities onto a space that minimizes big probabilities and maximizes small ones
    /// -log(p) is how we do this
    double distance;
    /// minimum-distance predecessor of this node
    uint64_t predecessor;
    /// whether or not this node has been investigated
    NodeColor color;
};

vector<uint64_t> Dijkstras(set<GraphNode, GNCompare> &nodes, uint64_t source, uint64_t sink)
{
    // maps a node ID to its dijkstra information
    map<uint64_t, DijkstraNode> DMap;
    // priority queue that holds all newly discovered nodes. Minimum paths get priority
    deque<GraphNode> Q;
    for (const auto &node : nodes)
    {
        // initialize each dijkstra node to have infinite distance, itself as its predecessor, and the unvisited nodecolor
        DMap[node.NID] = DijkstraNode(INFINITY, node.NID, NodeColor::White);
    }
    DMap[source].color = NodeColor::Grey;
    DMap[source].distance = 0;
    Q.push_back(*(nodes.find(source)));
    while (!Q.empty())
    {
        // for each neighbor of u, calculate the neighbors new distance
        for (const auto &neighbor : Q.front().neighbors)
        {
            if (neighbor.first == source)
            {
                // we've found a loop
                // the DMap distance will be 0 so we can't do a comparison of distances on the first go-round
                // if its not the first go-round then a comparison is fair game
                if (DMap[source].predecessor == source)
                {
                    DMap[source].predecessor = Q.front().NID;
                    DMap[source].distance = -log(neighbor.second.second) + DMap[Q.front().NID].distance;
                }
            }
            if (-log(neighbor.second.second) + DMap[Q.front().NID].distance < DMap[neighbor.first].distance)
            {
                DMap[neighbor.first].distance = -log(neighbor.second.second) + DMap[Q.front().NID].distance;
                DMap[neighbor.first].predecessor = Q.front().NID;
                if (DMap[neighbor.first].color == NodeColor::White)
                {
                    Q.push_back(*(nodes.find(neighbor.first)));
                    DMap[neighbor.first].color = NodeColor::Grey;
                }
                else if (DMap[neighbor.first].color == NodeColor::Grey)
                {
                    // we've already seen this node, update its predecessor
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
            if (DN.second.predecessor == sink)
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
    auto csvData = LoadBIN(InputFilename);

    spdlog::trace("Loading bin file: {0}", InputFilename);
    set<GraphNode, GNCompare> nodes;
    fstream inputFile;
    inputFile.open(InputFilename, ios::in | ios::binary);
    while (inputFile.peek() != EOF)
    {
        // New block description: BBID,#ofNeighbors (16 bytes per neighber)
        uint64_t key;
        inputFile.readsome((char *)&key, sizeof(uint64_t));
        GraphNode currentNode;
        if (nodes.find(key) == nodes.end())
        {
            currentNode = GraphNode(key);
            // right now, NID and blocks are 1to1
            currentNode.blocks.insert(key);
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
        /*spdlog::info("Examining node " + to_string(node.NID));
        for (const auto &neighbor : node.neighbors)
        {
            spdlog::info("Neighbor " + to_string(neighbor.first) + " has instance count " + to_string(neighbor.second.first) + " and probability " + to_string(neighbor.second.second));
        }
        cout << endl;*/
        spdlog::info("Node "+to_string(node.NID)+" has "+to_string(node.neighbors.size())+" neighbors.");
    }

    // find minimum cycles
    bool notDone = true;
    size_t numKernels = 0;
    set<MinKernel, MKCompare> minKernels;
    while (notDone)
    {
        notDone = false;
        for (const auto &node : nodes)
        {
            auto nodeIDs = Dijkstras(nodes, node.NID, node.NID);
            if (!nodeIDs.empty())
            {
                auto newKernel = MinKernel();
                for (const auto &id : nodeIDs)
                {
                    newKernel.nodes.insert(*(nodes.find(id)));
                }
                minKernels.insert(newKernel);
            }
        }
        if (minKernels.size() == 0)
        {
            notDone = false;
        }
        else
        {
            notDone = true;
            numKernels += minKernels.size();
        }
        break;
    }
    for (const auto &kernel : minKernels)
    {
        spdlog::info("MinKernel " + to_string(kernel.KID) + " has " + to_string(kernel.getBlocks().size()) + " blocks:");
        for (const auto &block : kernel.getBlocks())
        {
            spdlog::info(to_string(block));
        }
        cout << endl;
    }
    return 0;
}