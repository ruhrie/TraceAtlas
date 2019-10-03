import zlib
import operator
import os
import math
from collections import defaultdict
from Status import ConsoleStatus as cs

def ExtractKernels(sourceFile, kernels, newLine = False):
    print("Extracting type two kernels")
    blockSize = 4096
    kernStart = defaultdict(lambda:None)
    finalBlocks = defaultdict(lambda: set())
    blocks = defaultdict(lambda:set())
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

                sepperated = line.split(':')
                key = sepperated[0]
                value = sepperated[1]
                if key == "BasicBlock":
                    block = int(value, 0)
                    for index in range(len(kernels)):
                        kernel = kernels[index]
                        if block == kernStart[index]:
                            blocks[index].clear()
                        else:
                            blocks[index].add(block)
                            if block in kernel:
                                if kernStart[index] == None:
                                    kernStart[index] = block
                                    finalBlocks[index].add(kernStart[index])
                                    blocks[index].clear()
                                else:
                                    finalBlocks[index] |= blocks[index]
                                    blocks[index].clear()
            priorLine = lines[-1]
            j += 1
            if j % 10 == 0:
                lbar.Update(j)
            if len(data) != blockSize:
                lbar.Finish()
                break 

    checker = []
    for index in range(len(kernels)):
        if not finalBlocks[index] in checker:
            checker.append(finalBlocks[index])
    finalDict = dict()
    for index in range(len(checker)):
        finalDict[index] = list(sorted(checker[index]))
    print("Detected " + str(len(finalDict)) + " unique type two kernels")
    return finalDict