import argparse
import zlib
import operator

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("-i", "--input_file", default="raw.trc", help="Trace file to be read")
arg_parser.add_argument("-t", "--threshhold", default=0.95, help="The threshhold of code coverage required in the program")
arg_parser.add_argument("-p", "--probability_file", default="probabilities.csv", help="The probability file")
arg_parser.add_argument("-k", "--kernel_file", default="kernels.csv", help="The kernel file")
arg_parser.add_argument("-ht", "--hot_threshhold", default=512, help="Minimum instance count for kernel")
arg_parser.add_argument("-ck", "--chronological_kernel", help="Chronological kernel file")

args = arg_parser.parse_args()
input = args.input_file
probF = args.probability_file
kernF = args.kernel_file
hotThresh = args.hot_threshhold
thresh = args.threshhold
chronKern = args.chronological_kernel

radius = 5
blockSize = 4096
finalKernels = []
with open(input, 'rb') as stream:
    decob = zlib.decompressobj()
    priorLine = ""
    version = ''
    priorBlocks = []
    blockMap = dict()
    blockCount = dict()
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
                    elif split[0] == 'function':
                        try:
                            function = int(split[1])
                        except ValueError:
                            raise Exception('Function attribute on line ' + str(split[1]) + ' is not an integer')
                    else:
                        raise Exception('Unrecognized tag: ' + split[0])
            #now have attributes
            if len(priorBlocks) > 0 and block == priorBlocks[-1]:
                continue
            if not block in blockMap.keys():
                blockMap[block] = dict()
                blockCount[block] = 0
            blockCount[block] += 1
            priorBlocks.append(block)
            if len(priorBlocks) > (2 * radius + 1):
                priorBlocks.remove(priorBlocks[0])
                target = priorBlocks[-radius]
            if len(priorBlocks) > radius:
                for j in range(len(priorBlocks)):
                    target = priorBlocks[j]
                    if not target in blockMap[block].keys():
                        blockMap[block][target] = 0
                    blockMap[block][target] += 1
                        
            
        if len(data) != blockSize:
            break

    #now get the probabilities
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
    
    for kernel in kernels:
        foundIndeces = []
        for block in kernel:
            if block in kernelMap.keys():
                index = kernelMap[block]
                if not index in foundIndeces:
                    foundIndeces.append(index)
        if len(foundIndeces) == 0:
            for block in kernel:
                kernelMap[block] = len(finalKernels)
            finalKernels.append(kernel)
        elif len(foundIndeces) == 1:
            continue
        else:
            raise "Kernel fusion required. Unimplemented"
    with open(kernF, "w") as outFile:
        for kernel in finalKernels:
            for j in range(len(kernel)):
                kern = kernel[j]
                outFile.write(str(kern))
                if(j != (len(kernel) - 1)):
                    outFile.write(",")
            outFile.write("\n")
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
    


if chronKern != None:
    kernelPaths = dict()
    kernStart = dict()
    kernAppend = dict()
    for kernel in finalKernels:
        kernStart[str(kernel)] = False
        kernAppend[str(kernel)] = set()
        kernelPaths[str(kernel)] = set()
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
                        elif split[0] == 'function':
                            try:
                                function = int(split[1])
                            except ValueError:
                                raise Exception('Function attribute on line ' + str(split[1]) + ' is not an integer')
                        else:
                            raise Exception('Unrecognized tag: ' + split[0])
                #now have attributes

                for kernel in finalKernels:
                    if block in kernel:
                        kernAppend[str(kernel)].add(block)
                        if kernStart[str(kernel)]:
                            kernelPaths[str(kernel)] |= kernAppend[str(kernel)]
                            kernAppend[str(kernel)].clear()
                        kernStart[str(kernel)] = True
                    else:
                        if kernStart[str(kernel)]:
                            kernAppend[str(kernel)].add(block)
            if len(data) != blockSize:
                break
    with open(chronKern, "w") as outFile:
        for kernel in finalKernels:
            toWrite = ""
            for kern in kernAppend[str(kernel)]:
                toWrite += str(kern) + ","
            outFile.write(toWrite[:-1])
            outFile.write("\n")
