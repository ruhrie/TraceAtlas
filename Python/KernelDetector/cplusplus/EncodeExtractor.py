import zlib
import operator
import os
import math
from collections import defaultdict
from Status import ConsoleStatus as cs

def ExtractKernels(sourceFile, kernels, newLine = False):
    # kernels is a list of lists, where each entry is a list of BBIDs for that kernel
    print("Extracting type two kernels")
    blockSize = 4096
    kernStart = defaultdict(lambda:None) # holds the first blockID of each type 1 kernel. This BBID must belong to a type 1 kernel
    finalBlocks = defaultdict(lambda: set()) # holds the final set of blockIDs for each type 2 kernel 
    blocks = defaultdict(lambda:set()) # holds temporary sets of blocks that didnt belong to a type 1 kernel
    traceSize = os.path.getsize(sourceFile)
    blockCount = math.ceil(traceSize / blockSize)

    with open(sourceFile, 'rb') as stream:
        lbar = cs(newLine)
        lbar.Start()
        lbar.SetMax(blockCount)
        j = 0
        lbar.Start()
        decob = zlib.decompressobj()
        priorLine = ""
        while(True):
            data = stream.read(blockSize)
            result = decob.decompress(data).decode('utf-8')
            lines = result.split("\n")
            for line in lines[:-1]:
                if line == lines[0]:
                    line = priorLine + line
                #line is now formatted. Do the logic
                # same as EncodeExtract to this point

                # separate line into key and value
                sepperated = line.split(':')
                key = sepperated[0]
                value = sepperated[1]
                if key == "BasicBlock":
                    # if we have a basic block, get its ID
                    block = int(value, 0)
                    for index in range(len(kernels)): # when we find a block, check in everry type 1 kernel for it
                        kernel = kernels[index]
                        if block == kernStart[index]: # if we've already seen this block associated with this index, clean our tmp block list,
                                                      # because the kernel must have cycled and is now starting again
                            blocks[index].clear()
                        else: # else add it to our temporary set
                            blocks[index].add(block)
                            if block in kernel: # if this basic block belongs to this type 1 kernel
                                if kernStart[index] == None: # if we haven't seen this type 1 kernel before, initialize its start, add it to 
                                                             # final set as well, and clear our temporary set
                                    kernStart[index] = block
                                    finalBlocks[index].add(kernStart[index]) 
                                    blocks[index].clear()
                                else: # else we have a blockID outside this type 1 kernel, add it to our final blocks set and clear our tmp set
                                    finalBlocks[index] |= blocks[index]
                                    blocks[index].clear()
            priorLine = lines[-1]
            j += 1
            if j % 10 == 0:
                lbar.Update(j)
            if len(data) != blockSize:
                lbar.Finish()
                break 

    checker = [] # list of sets
    for index in range(len(kernels)): # for each type 1 kernel
        if not finalBlocks[index] in checker: # Create a list of sets, each belonging to a type 1 kernel, that contain  
            checker.append(finalBlocks[index])
    finalDict = dict()
    for index in range(len(checker)): # for every set of blockIDs belonging to a type 1 kernels, assign a sorted list to that index in finalDict
        finalDict[index] = list(sorted(checker[index])) 
    print("Detected " + str(len(finalDict)) + " unique type two kernels")
    return finalDict
