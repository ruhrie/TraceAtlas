from Classes.Op import Op
class BrOp(Op):
    def __init__(self, line, block, expr):
        Op.__init__(self, line, block)
        spaceSplit = expr.split(" ")
        if spaceSplit[1] == "label":
            self.target = spaceSplit[2]
        else:
            raise Exception("Improperly formatted br node: " + expr)
