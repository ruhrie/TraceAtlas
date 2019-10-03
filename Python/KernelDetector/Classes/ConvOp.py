from Classes.Op import Op
class ConvOp(Op):
    def __init__(self, line, block, address, ultimaValue, typeToValue):
        self.ultimaValue = ultimaValue
        self.typeToValue = typeToValue
        Op.__init__(self, line, block)
