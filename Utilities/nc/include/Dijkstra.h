#pragma once
#include <stdint.h>
#include <vector>

std::vector<uint64_t> Dijkstra(std::vector<std::vector<float>> graph, uint64_t start, uint64_t end);
