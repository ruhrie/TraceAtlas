import argparse
import zlib
import operator
import json

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("-i", "--input_file", default="raw.trc", help="Trace file to be read")
arg_parser.add_argument("-k", "--kernel_file", default="kernels.csv", help="The kernel file")
arg_parser.add_argument("-o", "--output_file", default="kernel.json", help="Output kernel file")
arg_parser.add_argument("-d", "--dag_file", default=None, help="Ouput dag json file")

args = arg_parser.parse_args()
input = args.input_file
kernF = args.kernel_file
outFile = args.output_file
dag = args.dag_file
kernels = []
with open(kernF, 'r') as stream:
    data = stream.read()
    lines = data.split("\n")
    for line in lines:
        blocks = line.split(',')
        kernels.append(blocks)

blockSize = 4096
kernStart = dict()
finalBlocks = dict()
blocks = dict()
memoryDict = dict()
consumerDict = dict()
with open(input, 'rb') as stream:
    decob = zlib.decompressobj()
    priorLine = ""
    version = ''
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
            #line is now formatted. Do the logic
            if line[:2] != "  ": #its a label
                continue
            sepperated = line.split(';')
            inst = sepperated[0].strip()
            if len(sepperated) > 1:
                for i in range(1, len(sepperated)):
                    split = sepperated[i].split(':')
                    if split[0] == 'version':
                        if version != '':
                            raise Exception('Version tag duplicated')
                        else:
                            version = split[1]
                    elif split[0] == 'line':
                        try:
                            line = int(split[1])
                        except ValueError:
                            raise Exception('Line attribute on line ' + str(split[1]) + ' is not an integer')
                    elif split[0] == 'block':
                        try:
                            block = int(split[1])
                        except ValueError:
                            raise Exception('Block attribute on line ' + str(split[1]) + ' is not an integer')
                    elif split[0] == 'address':
                        try:
                            address = int(split[1])
                        except ValueError:
                            raise Exception('Address attribute on line ' + str(split[1]) + ' is not an integer')
                    elif split[0] == 'function':
                        try:
                            function = int(split[1])
                        except ValueError:
                            raise Exception('Function attribute on line ' + str(split[1]) + ' is not an integer')
                    else:
                        raise Exception('Unrecognized tag: ' + split[0])
            if inst.startswith("store"):
                memoryDict[address] = block
            elif "=" in inst:
                spl = inst.split("=")
                if spl[1].strip().startswith("load"):
                    if not block in consumerDict.keys():
                        consumerDict[block] = set()
                    if address in memoryDict.keys():
                        consumerDict[block].add(memoryDict[address])
                    else:
                        consumerDict[block].add(-1)
            for index in range(len(kernels)):
                kernel = kernels[index]
                if not index in kernStart.keys():
                    kernStart[index] = None
                if not index in blocks.keys():
                    blocks[index] = set()
                if not index in finalBlocks.keys():
                    finalBlocks[index] = set()
                if block == kernStart[index]:
                    blocks[index].clear()
                    continue
                blocks[index].add(block)
                if str(block) in kernel:
                    if kernStart[index] == None:
                        kernStart[index] = block
                        finalBlocks[index].add(kernStart[index])
                        blocks[index].clear()
                        continue
                    finalBlocks[index] |= blocks[index]
                    blocks[index].clear()
        if len(data) != blockSize:
            break
finalDict = dict()
for index in range(len(kernels)):
    finalDict[index] = list(finalBlocks[index])
with open(outFile, 'w') as fp:
    json.dump(finalDict, fp)
if dag != None:
    asdf = dict()
    #now actually generate the dag
    backRef = dict()
    test = dict()
    for index in range(len(finalBlocks)):
        kernel = finalBlocks[index]
        for block in kernel:
            backRef[block] = index
    for index in range(len(finalBlocks)):
        kernel = finalBlocks[index]
        asdf[index] = set()
        for block in kernel:
            if block in consumerDict.keys():
                others = consumerDict[block]
                for other in others:
                    if other in backRef.keys():
                        ext = backRef[other]
                        if ext != index:
                            asdf[index].add(ext)
                    else:
                        if other in consumerDict.keys():
                            fd = consumerDict[other]
                        asdf[index].add(-1*other)

    dagDict = dict()
    for key in asdf.keys():
        dagDict[key] = list(asdf[key])
    with open(dag, "w") as df:
        json.dump(dagDict, df)