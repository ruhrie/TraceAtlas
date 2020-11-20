#include <GraphTransforms.h>
#include <math.h>

using namespace std;

std::vector<std::vector<float>> ProbabilityTransform(std::vector<std::vector<uint64_t>> input)
{
    vector<vector<float>> result;

    for(int i = 0; i < input.size(); i++)
    {
        vector<float> newRow;
        float sum = 0.0f;
        for(int j = 0; j < input[i].size(); j++)
        {
            sum += input[i][j];
        }
        for(int j = 0; j < input[i].size(); j++)
        {
            newRow.push_back(-1 * log(input[i][j] / sum));
        }
        result.push_back(newRow);
    }

    return result;
}