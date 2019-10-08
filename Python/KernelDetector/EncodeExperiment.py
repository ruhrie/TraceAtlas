import argparse
import zlib
import operator
from Classes.Block import Block

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("-i", "--input_file", default="D:/trace/raw.trc", help="Trace file to be read")
arg_parser.add_argument("-t", "--threshhold", default=0.95, help="The threshhold of code coverage required in the program")
arg_parser.add_argument("-p", "--probability_file", default=None, help="The probability file")
arg_parser.add_argument("-k", "--kernel_file", default="kernels.csv", help="The kernel file")
arg_parser.add_argument("-ht", "--hot_threshhold", default=512, help="Minimum instance count for kernel")

args = arg_parser.parse_args()
input = args.input_file
probF = args.probability_file
kernF = args.kernel_file
hotThresh = args.hot_threshhold
thresh = args.threshhold
radius = 5
blockSize = 4096
finalKernels = []




newTest = []
memoryDict = dict()

with open(input, 'rb') as stream:
    decob = zlib.decompressobj()
    priorLine = ""
    version = ''
    priorBlocks = []
    blockMap = dict()
    blockCount = dict()
    block = None
    while(True):
        data = stream.read(blockSize)
        result = decob.decompress(data).decode('utf-8')
        lines = result.split("\n")
        for i in range(len(lines)):
            line = lines[i]
            if i == 0:
                line = priorLine + line
            if i == len(lines) - 1:
                priorLine = line
                continue
            spl = line.split(":")
            key = spl[0]
            val = spl[1]
            if key == "TraceVersion":
                version = val
            elif key == "BasicBlock":
                blockInt = int(val, 0)
                if block != None:
                    fin = str(block)

                    if not fin in blockMap.keys():
                        blockMap[fin] = dict()
                        blockCount[fin] = 0
                    blockCount[fin] += 1
                    priorBlocks.append(fin)
                    if len(priorBlocks) > (2 * radius + 1):
                        priorBlocks.remove(priorBlocks[0])
                        target = priorBlocks[-radius]
                    if len(priorBlocks) > radius:
                        for j in range(len(priorBlocks)):
                            target = priorBlocks[j]
                            if not target in blockMap[fin].keys():
                                blockMap[fin][target] = 0
                            blockMap[fin][target] += 1


                block = Block(blockInt)
            elif key == "LoadAddress":
                load = int(val, 0)
                if load in memoryDict.keys():
                    block.Producers.add(memoryDict[load])
                else:
                    block.Producers.add(-1)
            elif key == "StoreAddress":
                store = int(val, 0)
                memoryDict[store] = block.Block
            else:
                raise Exception("Unrecognized key: " + key)

        if len(data) != blockSize:
            break

    kernels = []
    for key in blockMap:
        total = 0
        for sub in blockMap[key]:
            total += blockMap[key][sub]
        for sub in blockMap[key]:
            blockMap[key][sub] = float(blockMap[key][sub]) / float(total)
    for block in blockCount:
        count = blockCount[block]
        if count > hotThresh:
            sum = 0.0
            values = blockMap[block]
            sValues = sorted(values.items(), key=operator.itemgetter(1), reverse=True)
            kernel = []
            while sum < thresh:
                entry = sValues[0]
                sValues.remove(sValues[0])
                sum += entry[1]
                kernel.append(entry[0])
            kernels.append(kernel)
    kernelMap = dict()
    
    sets = []

    for kernel in kernels:
        temp = set(kernel)
        if not temp in sets:
            sets.append(temp)
    with open(kernF, "w") as outFile:
        for kern in sets:
            kernel = list(kern)
            for j in range(len(kernel)):
                kern = kernel[j]
                outFile.write(str(kern))
                if(j != (len(kernel) - 1)):
                    outFile.write(",")
            outFile.write("\n")
    if probF != None:
        with open(probF, "w") as outFile:
            blocks = []
            for block in blockCount.keys():
                outFile.write("," + str(block))
                blocks.append(block)
            outFile.write("\n")
            for block in blocks:
                outFile.write(str(block))
                for subblock in blocks:
                    if subblock in blockMap[block].keys():
                        outFile.write(","+ str(blockMap[block][subblock]))
                    else:
                        outFile.write(",0")
                outFile.write("\n")
    