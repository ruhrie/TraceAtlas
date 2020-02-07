import csv

# class MemoryGroup:
#     def __init__(self):
#         self.frequency = 0
#         self.beginAddress = 0
#         self.endAddress = 0
#         self.memorySize = 0
#         self.index = 0
# memoryGroup = MemoryGroup()
# memoryGroup.__init__()
memoryGroup = []
# index definition
frequency_index = 0
beginAddress_index = 1
endAddress_index = 2
memorySize_index = 3
index_index = 4
memoryLine = [0] * 8
file = open('raw.trc', 'rt')
address = 0
unitSize = 4
total_number = 0
for line in file.readlines():
    line = line.split(':')
    try:
        address = int(line[1])
    except:
        print("wrong address %s", line[1])
    memoryLine = [0] * 8
    if memoryGroup.__sizeof__() > 1:
        for memLine_itr in memoryGroup:
            if memLine_itr[endAddress_index] > address > memLine_itr[beginAddress_index]:
                memLine_itr[frequency_index] += 1
                memLine_itr[memorySize_index] += unitSize
            elif address > memLine_itr[endAddress_index] and address - memLine_itr[endAddress_index] < unitSize * 2:
                memLine_itr[frequency_index] += 1
                memLine_itr[memorySize_index] += unitSize
                memLine_itr[endAddress_index] = address
            elif address < memLine_itr[beginAddress_index] and memLine_itr[beginAddress_index] - address < unitSize * 2:
                memLine_itr[frequency_index] += 1
                memLine_itr[memorySize_index] += unitSize
                memLine_itr[beginAddress_index] = address
            else:
                memoryLine[frequency_index] = 1
                memoryLine[beginAddress_index] = address
                memoryLine[endAddress_index] = address + unitSize
                memoryLine[memorySize_index] = unitSize
                memoryLine[index_index] = total_number + 1
                total_number = memoryLine[index_index]
                memoryGroup.append(memoryLine)
    else:
        memoryLine[frequency_index] = 1
        memoryLine[beginAddress_index] = address
        memoryLine[endAddress_index] = address + unitSize
        memoryLine[memorySize_index] = unitSize
        memoryLine[index_index] = total_number + 1
        total_number = memoryLine[index_index]
        memoryGroup.append(memoryLine)
with open('eggs.csv', 'w', newline='') as csvFile:
    memWriter = csv.writer(csvFile, delimiter=' ', quotechar='|', quoting=csv.QUOTE_MINIMAL)
    memWriter.writerow(['Spam'] * 5 + ['Baked Beans'])
    memWriter.writerow(['Spam', 'Lovely Spam', 'Wonderful Spam'])
