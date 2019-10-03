class Block(object):
    def __init__(self, block):
        self.Block = block
        self.Producers = set()
    def __str__(self):
        result = str(self.Block)
        sort = sorted(self.Producers)
        for entry in sort:
            result += "_" + str(entry)
        return result
    def __eq__(self, other):
        if other == None:
            return False
        return (self.Block == other.Block) and (self.Producers == other.Producers)