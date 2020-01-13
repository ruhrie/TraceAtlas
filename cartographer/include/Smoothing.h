#pragma once
#include <set>
#include <map>
#include <string>

std::map<int, std::set<int>> SmoothKernel(std::map<int, std::set<int>> blocks, std::string bitcodeFile);
