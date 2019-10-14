import zlib
import operator
import os
import math
from collections import defaultdict
from collections import deque
from Status import ConsoleStatus as cs

def DetectKernels(sourceFile, thresh = 0.95, hotThresh = 512, newLine = False):
    print("Detecting type one kernels")
    result = []
    radius = 5
    blockSize = 4096

    blockMap = defaultdict(lambda:defaultdict(lambda:0))
    blockCount = defaultdict(lambda:0)
    traceSize = os.path.getsize(sourceFile)
    blocks = math.ceil(traceSize / blockSize)

    with open(sourceFile, 'rb') as stream:
        index = 0
        decob = zlib.decompressobj()
        priorLine = ""
        priorBlocks = deque()
        bar = cs(newLine)
        bar.SetMax(blocks)
        bar.Start()
        while(True):
            data = stream.read(blockSize)
            res = decob.decompress(data).decode('utf-8')
            lines = res.split("\n")
            for line in lines[:-1]:
                if line == lines[0]:
                    line = priorLine + line
                spl = line.split(":")
                key = spl[0]
                val = spl[1]
                if key == "BasicBlock":
                    block = int(val, 0)
                    blockCount[block] += 1
                    priorBlocks.append(block)
                    if len(priorBlocks) > (2 * radius + 1):
                        priorBlocks.popleft()
                    if len(priorBlocks) > radius:
                        for j in priorBlocks:
                            blockMap[block][j] += 1
            priorLine = lines[-1]
            index += 1
            if index % 100 == 0:
                bar.Update(index)
            if len(data) != blockSize:
                bar.Finish()
                break
    kernels = []
    for key in blockMap:
        total = 0
        for sub in blockMap[key]:
            total += blockMap[key][sub]
        for sub in blockMap[key]:
            blockMap[key][sub] = float(blockMap[key][sub]) / float(total)
    covered = set()
    for block, count in sorted(blockCount.items(), key=lambda item:item[1], reverse=True):
        if count > hotThresh:
            if not block in covered:
                sum = 0.0
                values = blockMap[block]
                sValues = sorted(values.items(), key=operator.itemgetter(1), reverse=True)
                kernel = set()
                while sum < thresh:
                    entry = sValues[0]
                    covered.add(entry[0])
                    sValues.remove(sValues[0])
                    sum += entry[1]
                    kernel.add(entry[0])
                kernels.append(kernel)
        else:
            break
    
    for kernel in kernels:
        if not kernel in result:
            result.append(kernel)
    print("Detected " + str(len(result)) + " type one kernels")
    return result
