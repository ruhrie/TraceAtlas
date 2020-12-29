#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unordered_map>

using namespace std;

class dict
{
public:
    unordered_map<uint64_t, unordered_map<uint64_t, uint64_t>> base;
    ~dict()
    {
        char *tfn = getenv("MARKOV_FILE");
        string fileName;
        if (tfn != nullptr)
        {
            fileName = tfn;
        }
        else
        {
            fileName = "markov.bin";
        }
        ofstream fp(fileName, std::ios::out | std::ios::binary);
        for (auto addr1 : base)
        {
            fp.write((const char *)&addr1.first, sizeof(uint64_t));
            uint64_t length = addr1.second.size();
            fp.write((const char *)&length, sizeof(std::size_t));
            for (auto addr2 : addr1.second)
            {
                fp.write((const char *)&addr2.first, sizeof(uint64_t));
                fp.write((const char *)&addr2.second, sizeof(std::size_t));
            }
        }
        fp.close();
    }
};

uint64_t b;
uint64_t *markovResult;
bool markovInit = false;
dict TraceAtlasMarkovMap;

extern "C"
{
    extern uint64_t MarkovBlockCount;
    void MarkovIncrement(uint64_t a)
    {
        if (markovInit)
        {
            TraceAtlasMarkovMap.base[b][a]++;
        }
        else
        {
            markovInit = true;
        }
        b = a;
    }
}