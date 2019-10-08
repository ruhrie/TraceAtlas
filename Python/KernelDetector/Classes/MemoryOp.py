from Classes.Op import Op
class MemoryOp(Op):
    def __init__(self, line, block, address):
        self.address = address
        Op.__init__(self, line, block)


