from Classes.MemoryOp import MemoryOp
class StoreOp(MemoryOp):
     def __init__(self, line, block, inst, address):
        MemoryOp.__init__(self, line, block, address)
        comSplit = inst.split(",")
        for i in range(len(comSplit)):
            line = comSplit[i]
            spaceSplit = line.strip().split(" ")
            if i == 0:
                self.storeType = spaceSplit[1]
                self.storeVal = spaceSplit[2]
            elif i == 1:
                self.returnType = spaceSplit[0]
                self.returnVal = spaceSplit[1]
            elif i == 2:
                self.align = spaceSplit[1]
            else:
                raise Exception("Illegally formatted store expression: " + line)


