from Classes.Op import Op
class AluOp(Op):
    def __init__(self, line, block, address, ultimaValue, archValue):
        self.ultimaValue = ultimaValue
        self.archValue = archValue
        Op.__init__(self, line, block)
