#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>

std::ifstream::pos_type filesize(std::string filename);
std::vector<std::set<int>> DetectKernels(std::string sourceFile, float thresh, int hotThreash);
extern std::map<int, int> blockCount;