#pragma once
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

std::ifstream::pos_type filesize(std::string filename);
std::vector<std::set<int>> DetectKernels(std::string sourceFile, float thresh, int hotThreash);
extern std::map<int, uint64_t> blockCount;