from Classes.Op import Op
class ConditionalOp(Op):
    def __init__(self, line, block, address, ultimaValue, archValue, condition):
        self.ultimaValue = ultimaValue
        self.archValue = archValue
        self.condition = condition
        Op.__init__(self, line, block)
