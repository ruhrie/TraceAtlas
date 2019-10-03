import uuid
class KernelInstance(object):
    """description of class"""
    def __init__(self, block):
        self.block = block
        self.reads = set()
        self.writes = set()
        self.parents = set()
        self.UID = uuid.uuid4()

