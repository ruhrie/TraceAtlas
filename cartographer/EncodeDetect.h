
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <set>

std::ifstream::pos_type filesize(std::string filename);
std::vector<std::set<int>> DetectKernels(std::string sourceFile, float thresh, int hotThreash, bool newline);
