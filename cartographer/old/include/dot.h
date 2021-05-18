#pragma once
#include <set>
#include <string>

std::string GenerateDot(const std::set<std::set<int64_t>> &kernels);