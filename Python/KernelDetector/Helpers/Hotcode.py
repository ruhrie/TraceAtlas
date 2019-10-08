import operator
from Classes.StoreOp import *
from Classes.LoadOp import *
import Helpers.CodeProbabilities as cp
import json
import csv
kernelThreshhold = 0.95

def detectHotcodeEdges(ops, threshhold, file):
    opCount = len(ops)
    blockCount = dict() #we are enumerating the number of times a block occurs
    for op in ops:
        if(not op.block in blockCount.keys()):
            blockCount[op.block] = 0
        blockCount[op.block] += 1

    sortedDict = sorted(blockCount.items(), key=operator.itemgetter(1), reverse=1)#we sort by block found here so we can increment blocks up until we reach the threshhold
    ratio = 0
    hotCodeBlocks = []
    codeBlocks = []
    for pair in sortedDict:
        ratio += pair[1] / opCount
        hotCodeBlocks.append(pair[0])
        if(ratio >= threshhold):
            break
    #these dicts just store the load/store information, some of which is for debugging
    #it is referenced by block in the block dictionary and the target is a variable name seperation
    blockStores = dict()
    blockLoads = dict()
    storeTargets = dict()
    loadTargets = dict()
    fullBlockStores = dict()
    fullBlockLoads = dict()
    fullStoreTargets = dict()
    fullLoadTargets = dict()
    for op in ops:
        if not op.block in codeBlocks:
            codeBlocks.append(op.block)

        if isinstance(op, StoreOp):
            address = op.address
            if not op.block in fullBlockStores.keys():
                fullBlockStores[op.block] = []
                fullStoreTargets[op.block] = []
            if not address in fullBlockStores[op.block] and address != None:
                fullBlockStores[op.block].append(address)
            if not op.returnVal in fullStoreTargets[op.block]:
                fullStoreTargets[op.block].append(op.returnVal)
        elif isinstance(op, LoadOp):
            address = op.address
            if not op.block in fullBlockLoads.keys():
                fullBlockLoads[op.block] = []
                fullLoadTargets[op.block] = []
            if not address in fullBlockLoads[op.block] and address != None:
                fullBlockLoads[op.block].append(address)
            if not op.addressVal in fullLoadTargets[op.block]:
                fullLoadTargets[op.block].append(op.addressVal)


        if op.block in hotCodeBlocks:
            if isinstance(op, StoreOp):
                address = op.address
                if not op.block in blockStores.keys():
                    blockStores[op.block] = []
                    storeTargets[op.block] = []
                if not address in blockStores[op.block] and address != None:
                    blockStores[op.block].append(address)
                if not op.returnVal in storeTargets[op.block]:
                    storeTargets[op.block].append(op.returnVal)
            elif isinstance(op, LoadOp):
                address = op.address
                if not op.block in blockLoads.keys():
                    blockLoads[op.block] = []
                    loadTargets[op.block] = []
                if not address in blockLoads[op.block] and address != None:
                    blockLoads[op.block].append(address)
                if not op.addressVal in loadTargets[op.block]:
                    loadTargets[op.block].append(op.addressVal)

    #just sorting the hotcode here by the usage percentage
    for i in range(len(hotCodeBlocks)):
        if hotCodeBlocks[i] in blockStores.keys():
            blockStores[hotCodeBlocks[i]].sort()
        if hotCodeBlocks[i] in blockLoads.keys():
            blockLoads[hotCodeBlocks[i]].sort()
    blockOrder = [] #we need block order in order to calculate the block probabilities
    fullBlockOrder = []
    for op in ops:
        if len(fullBlockOrder) == 0:
            fullBlockOrder.append(op.block)
        elif fullBlockOrder[-1] != op.block:
            fullBlockOrder.append(op.block)
        if op.block in hotCodeBlocks:
            if len(blockOrder) == 0:
                blockOrder.append(op.block)
            else:
                if blockOrder[-1] != op.block:
                    blockOrder.append(op.block)
    hotDag = generateDag(blockStores, blockLoads, hotCodeBlocks, ops) #generate the dag
    fullDag = generateDag(fullBlockStores, fullBlockLoads, codeBlocks, ops)
    kernels = identifyKernels(blockOrder, hotCodeBlocks) #get the actual kernels

    asdf = cp.getProbabilities(fullBlockOrder)

    outputFile = open("data.csv", "w")
    for key in asdf:
        outputFile.write(str(key[0]));
        outputFile.write(",")
        outputFile.write(str(key[1]));
        outputFile.write(",")
        outputFile.write(str(asdf[key]));
        outputFile.write("\n")
    outputFile.close()
    kernelBoundaries = []
    for kernel in kernels:

        firstIndex = -1
        lastIndex = -1
        for i in range(len(fullBlockOrder)):
            block = fullBlockOrder[i]
            if block in kernel:
                if firstIndex == -1:
                    firstIndex = i
                lastIndex = i
        vis = fullBlockOrder[firstIndex: lastIndex]
        testEntries = []
        for i in range(firstIndex, lastIndex):
            if not fullBlockOrder[i] in testEntries:
                testEntries.append(fullBlockOrder[i])

        
        kernelBoundaries.append(testEntries)
    outputFile = open(file, "w")
    for i in range(len(kernelBoundaries)):
        kernel = kernelBoundaries[i]
        for j in range(len(kernel)):
            if j != 0:
                outputFile.write(",")
            outputFile.write(str(kernel[j]))
        if(i != len(kernelBoundaries) - 1):
            outputFile.write("\n")
    outputFile.close()

            
    '''
    edges = identifyEdges(fullDag, kernels)

    dumpStores = dict()
    dumpLoads = dict()
    stores = dict()
    for op in ops:
        if isinstance(op, StoreOp):
            stores[op.address] = (op.block, op.line)
        elif isinstance(op, LoadOp):
            if op.address in stores.keys() and op.block in edges.keys():
                for edge in edges[op.block]:
                    if edge == stores[op.address][0]:
                        if stores[op.address][0] in hotCodeBlocks: #the store is the output of a kernel
                            if not stores[op.address][0] in dumpStores.keys():
                                dumpStores[stores[op.address][0]] = []
                            if not stores[op.address][1] in dumpStores[stores[op.address][0]]:
                                dumpStores[stores[op.address][0]].append(stores[op.address][1])
                        if op.block in hotCodeBlocks: #the load is the input of a kernel
                            if not op.block in dumpLoads.keys():
                                dumpLoads[op.block] = []
                            if not op.line in dumpLoads[op.block]:
                                dumpLoads[op.block].append(op.line)
    final = dict()
    final["Loads"] = dumpLoads
    final["Stores"] = dumpStores
    with open(file, "w") as jf:
        json.dump(final, jf)
    '''
    return


def generateDag(stores, loads, blocks, ops):
    #generate a dag for the memory accesses
    dag = dict()
    accesses = dict() # address is key, block is value
    for op in ops:
        if isinstance(op, StoreOp):
            accesses[op.address] = op.block
        elif isinstance(op, LoadOp):
            if not op.block in dag.keys() and op.block in blocks:
                dag[op.block] = []
            if op.address in accesses.keys() and op.block in dag.keys() and not accesses[op.address] in dag[op.block] and op.block != accesses[op.address] and op.block in blocks and accesses[op.address] in blocks:
                dag[op.block].append(accesses[op.address])
    return dag

def identifyKernels(blockOrder, hotBlocks):
    kernels = []
    probabilities = cp.getProbabilities(blockOrder) #get the individual probabilities (this may be modified)
    for hotBlock in hotBlocks:
        matchingProbabilities = dict() #get all the probabilities for this hotblock aka hotblock | X
        for prob in probabilities.keys():
            if prob[0] == hotBlock:
                matchingProbabilities[prob[1]] = probabilities[prob]
        sortedDict = sorted(matchingProbabilities.items(), key=operator.itemgetter(1), reverse=1) #now sor them so we can utilize them to identify kernels
        totalProb = 0
        #try and find the index and if it doesn't exist create it
        index = None
        for i in range(len(kernels)):
            if hotBlock in kernels[i]:
                index = i
        if index == None:
            kernels.append([hotBlock])
            index = len(kernels) - 1

        dictIndex = 0
        #enumerate the probabilities of blocks already in the kernel
        for block in kernels[index]:
            if (hotBlock, block) in probabilities:
                totalProb += probabilities[(hotBlock, block)]


        #now create a kernel by adding up code until we surpass 95% coverage
        while totalProb <= kernelThreshhold:
            entry = sortedDict[dictIndex]
            if not entry[0] in kernels[index]:
                kernels[index].append(entry[0])
                totalProb += entry[1]
            dictIndex += 1
    return kernels

def identifyEdges(dag, kernels):
    edges = dict()
    for block in dag.keys():
        index = None
        for i in range(len(kernels)):
            kernel = kernels[i]
            if block in kernel:
                index = i
                break
        if index == None:
            for dependency in dag[block]:
                for kernel in kernels:
                    if dependency in kernel:
                        if not block in edges.keys():
                            edges[block] = []
                        if not dependency in edges[block]:
                            edges[block].append(dependency)

        else:
            for dependency in dag[block]:
                if not dependency in kernels[index]:
                    if not block in edges.keys():
                        edges[block] = []
                    if not dependency in edges[block]:
                        edges[block].append(dependency)
    return edges