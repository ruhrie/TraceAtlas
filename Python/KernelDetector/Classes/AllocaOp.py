from Classes.Op import Op
class AllocaOp(Op):
    def __init__(self, line, block, inst):
        Op.__init__(self, line, block)
        eqSplit = inst.split("=")
        self.output = eqSplit[0].strip().strip("%").strip()
        comSplit = eqSplit[1].split(",")
        self.type = comSplit[0].strip().split(" ")[1].strip()
        self.align = comSplit[1].strip().split(" ")[1].strip()


