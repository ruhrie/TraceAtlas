#include "newCartographer.h"
#include "AtlasUtil/IO.h"
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>

using namespace llvm;
using namespace std;
using json = nlohmann::json;

cl::opt<string> InputFilename("i", cl::desc("Specify bin file"), cl::value_desc(".bin filename"), cl::Required);
cl::opt<string> BlockInfoFilename("i", cl::desc("Specify BlockInfo.json file"), cl::value_desc(".json filename"), cl::Required);
cl::opt<string> OutputFilename("o", cl::desc("Specify output json"), cl::value_desc("kernel filename"), cl::Required);

uint64_t GraphNode::nextNID = 0;
uint32_t Kernel::nextKID = 0;

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
        // TODO: select the neighbor with the shortest path to boost performance
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
        spdlog::info("Node " + to_string(node.NID) + " has " + to_string(node.neighbors.size()) + " neighbors.");
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
    outputJson["BlockCallers"] = 0;
    int id = 0;
    for (const auto &kernel : kernels)
    {
        outputJson["Kernels"][to_string(id)]["Blocks"] = kernel.getBlocks();
        outputJson["Kernels"][to_string(id)]["Labels"] = std::vector<string>();
        outputJson["Kernels"][to_string(id)]["Labels"].push_back("");
        id++;
    }
    ofstream oStream(OutputFilename);
    oStream << setw(4) << outputJson;
    oStream.close();
    return 0;
}