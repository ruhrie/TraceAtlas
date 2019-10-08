import argparse
import zlib
import operator
import json

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("-i", "--input_file", default="raw.trc", help="Trace file to be read")
arg_parser.add_argument("-k", "--kernel_file", default="kernels.csv", help="The kernel file")
arg_parser.add_argument("-ki", "--kernel_index", default=0, type=int, help="The kernel index to detect memory edges")
arg_parser.add_argument("-o", "--output_file", default="dag.json", help="Output dag file")
args = arg_parser.parse_args()
input = args.input_file
kernF = args.kernel_file
kernIndex = args.kernel_index
outFile = args.output_file
kernels = []
with open(kernF, 'r') as stream:
    data = stream.read()
    lines = data.split("\n")
    for line in lines:
        blocks = line.split(',')
        kernels.append(blocks)
blockSize = 4096
with open(input, 'rb') as stream:
    decob = zlib.decompressobj()
    priorLine = ""
    version = ''
    priorBlocks = []
    loadDict = dict()
    storeDict = dict()
    consumers = set()
    producers = set()
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
            inst = sepperated[0]
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
                    else:
                        raise Exception('Unrecognized tag: ' + split[0])
            eqSplit = inst.split("=")
            if(len(eqSplit) > 1):
                load = False
                store = False
                stripped = eqSplit[1].strip()
                if stripped.startswith("load"):
                    loadDict[address] = block
                    load = True
                if load:
                    if str(block) in kernels[kernIndex]:
                        producers.add(storeDict[address])
                    elif str(storeDict[address]) in kernels[kernIndex]:
                        consumers.add(block)
            else:
                stripped = eqSplit[0].strip()
                if stripped.startswith("store"):
                    storeDict[address] = block
                    store = True
        if len(data) != blockSize:
            break
    finalDict = dict()
    inputBlocks = set()
    outputBlocks = set()
    for block in consumers:
        for i in range(len(kernels)):
            kern = kernels[i]
            if str(block) in kern:
                outputBlocks.add(i)
    for block in producers:
        for i in range(len(kernels)):
            kern = kernels[i]
            if str(block) in kern:
                inputBlocks.add(i)
    finalDict["Inputs"] = list(inputBlocks)
    finalDict["Outputs"] = list(outputBlocks)
    with open(outFile, 'w') as fp:
        json.dump(finalDict, fp)