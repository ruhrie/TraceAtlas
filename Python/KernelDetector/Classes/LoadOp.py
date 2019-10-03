from Classes.MemoryOp import MemoryOp
class LoadOp(MemoryOp):
     def __init__(self, line, block, inst, address):
        MemoryOp.__init__(self, line, block, address)
        eqSplit = inst.split("=")
        self.target = eqSplit[0]
        comSplit = eqSplit[1].split(",")
        for i in range(len(comSplit)):
            line = comSplit[i]
            spaceSplit = line.strip().split(" ")
            if i == 0:
                self.loadType = spaceSplit[1]
            elif i == 1:
                self.addressType = spaceSplit[0]
                self.addressVal = spaceSplit[1]
            elif i == 2:
                self.align = spaceSplit[1]
            else:
                raise Exception("Illegally formatted load expression: " + line)


