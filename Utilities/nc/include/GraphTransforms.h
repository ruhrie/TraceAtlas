#pragma once
#include "AtlasUtil/Graph.h"
#include "Kernel.h"
#include <set>
#include <stdint.h>
#include <vector>

Graph<float> ProbabilityTransform(Graph<uint64_t> input);

Graph<float> GraphCollapse(Graph<float> base, const std::set<Kernel> &kernels);