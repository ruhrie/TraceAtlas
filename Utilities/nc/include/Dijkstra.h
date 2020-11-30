#pragma once
#include "AtlasUtil/Graph.h"
#include <stdint.h>
#include <vector>

std::vector<uint64_t> Dijkstra(Graph<float> graph, uint64_t start, uint64_t end);
