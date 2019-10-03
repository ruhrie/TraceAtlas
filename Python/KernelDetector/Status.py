from datetime import datetime
class ConsoleStatus(object):
    def __init__(self, lines=False):
        self.start_time = None
        self.max = 0
        if lines:
            self.LineFeed = "\n"
        else:
            self.LineFeed = "\r"
    def SetMax(self, max):
        self.max = max
    def Start(self):
        self.start_time = datetime.now()
    def Update(self, index):
        ratio = float(index / self.max)
        duration = datetime.now() - self.start_time
        print(self.LineFeed + "Reading block " + str(index) + " of " + str(self.max) + " | " + 
        str(round(ratio * 100, 2)) + "% | " + 
        "Elapsed Time: " + str(duration) + " | " + 
        "ETA: " + str(duration / ratio - duration)
        , end="")
    def Finish(self):
        duration = datetime.now() - self.start_time
        print("\nFinished in " + str(duration))