
sampleRange = 6
std = 2

def getProbabilities(blocks):
    probabilites = dict() #this is the final result in form (A,B) = A | B
    baseProbabilities = dict() #this is simply a dictionary of concatenated lists where is A is centered which appear in range |A+-5|
    totalBlocks = [] #just every block that appears
    for i in range(len(blocks)):
        block = blocks[i]
        if not block in totalBlocks: #enumerate all new blocks out
            totalBlocks.append(block)
        blocksPresent = blocks[max(0, i - 5):min(len(blocks), i + sampleRange)] #get all blocks witin sample range of the block
        if not block in baseProbabilities.keys(): #make sure the dictionary list is initialized
            baseProbabilities[block] = []
        baseProbabilities[block] += blocksPresent #and concatenate the lists
    for base in baseProbabilities.keys(): #we can now calculate the conditional probability for all blocks within the range, note that this would ideally be weighted by a gaussian but it doesn't appear necessary
        for block in totalBlocks:
            prob = baseProbabilities[base].count(block) / len(baseProbabilities[base])
            probabilites[(base,block)] = prob
    '''
    with open("prob.csv", "w") as jf:
        jf.write("base,neighbor,probability\n")
        for prob in probabilites:
            jf.write(str(prob[0]) + "," + str(prob[1]) + "," + str(probabilites[prob]) + "\n");
    '''
    return probabilites
