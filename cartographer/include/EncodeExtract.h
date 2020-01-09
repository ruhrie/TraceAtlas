#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

std::map<int, std::set<int>> ExtractKernels(std::string sourceFile, std::vector<std::set<int>> kernels, bool newline);
