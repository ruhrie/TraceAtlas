import zlib
import operator
import os
import math
from Classes.KernelInstance import KernelInstance
from Status import ConsoleStatus as cs

def GenerateDag(traceFile, typeTwoKernels, newLine = False):

    print("Starting dag detection")

    blockSize = 4096

    blockMap = dict()

    traceSize = os.path.getsize(traceFile)
    blocks = math.ceil(traceSize / blockSize)

    for key in typeTwoKernels:
        for entry in typeTwoKernels[key]:
            if not entry in blockMap.keys():
                blockMap[entry] = set()
            blockMap[entry].add(int(key,0))

    memoryDict = dict()

    inProg = dict()

    instances = []

    parents = dict()
    children = dict()

    for index in typeTwoKernels:
        parents[index] = []
        children[index] = []

    for index1 in typeTwoKernels:
        kernel1 = set(typeTwoKernels[index1])
        for index2 in typeTwoKernels:
            if index1 != index2:
                kernel2 = set(typeTwoKernels[index2])
                if kernel1.issuperset(kernel2):
                    children[index1].append(index2)
                    parents[index2].append(index1)

    with open(traceFile, 'rb') as stream:
        decob = zlib.decompressobj()
        priorLine = ""
        version = ''
        block = None
        loads = []
        stores = []
        currentKernels = set()
        index = 0
        bar = cs(newLine)
        bar.SetMax(blocks)
        bar.Start()
        k = 0
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
                    priorBlock = block
                    block = int(val, 0)
                    if block in blockMap.keys():
                            newKernels = blockMap[block]
                            removed = currentKernels - newKernels
                            added = newKernels - currentKernels
                            if len(removed) != 0:
                                for rem in removed:
                                    instances.append(inProg[rem])
                                    del inProg[rem]
                            if len(added) != 0:
                                for add in added:
                                    inst = KernelInstance(add)
                                    inProg[add] = inst
                            currentKernels = newKernels
                    loads = []
                    stores = []
                    index += 1
                elif key == "LoadAddress":
                    load = int(val, 0)
                    loads.append(load)
                    if block in blockMap.keys():
                        for kernel in blockMap[block]:
                            inProg[kernel].reads.add(load)
                            if load in memoryDict.keys():
                                for entry in memoryDict[load]:
                                    if entry != inProg[kernel].UID:
                                        inProg[kernel].parents.add(entry)
                elif key == "StoreAddress":
                    store = int(val, 0)
                    stores.append(store)
                    memoryDict[store] = set()
                    if block in blockMap.keys():
                        for kernel in blockMap[block]:
                            inProg[kernel].writes.add(store)
                            memoryDict[store].add(inProg[kernel].UID)
                else:
                    raise Exception("Unrecognized key: " + key)
            k += 1
            if k % 10 == 0:
                bar.Update(k)
            if len(data) != blockSize:
                for kern in currentKernels:
                    instances.append(inProg[kern])
                    del inProg[kern]
                bar.Finish()
                break

    return instances   
