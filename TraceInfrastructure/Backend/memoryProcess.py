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
# memoryGroup = []
# index definition
# frequency_index = 0
# beginAddress_index = 1
# endAddress_index = 2
# memorySize_index = 3
# index_index = 4
# memoryLine = [0] * 8
# unitSize = 4
# total_number = 0


def LookupMemoryTimingGroup(memoryTimingGroup_i, memoryTimingLine_i, repeatGroup_i):
    repeatLine = [0] * 4
    result_flag_t = 0  # 0:no result, 1:got it
    result_flag_r = 0
    for linet in memoryTimingGroup_i:
        if linet[0] == memoryTimingLine_i[0]:
            repeatLine[0] = memoryTimingLine_i[0]  # address
            repeatLine[1] = 1  # repeat number
            repeatLine[2] = memoryTimingLine_i[1]  # last time
            repeatLine[3] = memoryTimingLine_i[1] - linet[1]  # repeat time difference
            repeatGroup_i.append(repeatLine)
            memoryTimingGroup_i.remove(linet)
            result_flag_t = 1
    if result_flag_t == 0:
        if repeatGroup_i.__len__() > 0:
            for liner in repeatGroup_i:
                if liner[0] == memoryTimingLine_i[0]:
                    newTiming = memoryTimingLine_i[1] - liner[2]
                    for liner_inner in liner[3:]:
                        if newTiming == liner_inner:
                            liner[1] += 1
                            liner[2] = memoryTimingLine_i[1]
                            result_flag_r = 1
                    if result_flag_r == 0:
                        liner[1] += 1
                        liner[2] = memoryTimingLine_i[1]
                        liner.append(newTiming)
                        result_flag_r = 1
    if result_flag_r == 0 and result_flag_t == 0:
        memoryTimingGroup_i.append(memoryTimingLine_i)


memoryTimingGroup = []
repeatGroup = []
file = open('raw.trc', 'rt')
address = 0
timing = 0
for line in file.readlines():
    timing += 1
    memoryTimingLine = [0] * 2
    line = line.split(':')
    try:
        address = int(line[1])
    except:
        print("wrong address %s", line[1])
    memoryTimingLine[0] = address
    memoryTimingLine[1] = timing
    if memoryTimingGroup.__len__() > 0:
        LookupMemoryTimingGroup(memoryTimingGroup, memoryTimingLine, repeatGroup)
    else:
        memoryTimingGroup.append(memoryTimingLine)
all = 1
    # memoryLine = [0] * 8
    # if memoryGroup.__len__() > 1:
    #     for memLine_itr in memoryGroup:
    #         if memLine_itr[endAddress_index] > address > memLine_itr[beginAddress_index]:
    #             memLine_itr[frequency_index] += 1
    #             memLine_itr[memorySize_index] += unitSize
    #         elif address > memLine_itr[endAddress_index] and address - memLine_itr[endAddress_index] < unitSize * 2:
    #             memLine_itr[frequency_index] += 1
    #             memLine_itr[memorySize_index] += unitSize
    #             memLine_itr[endAddress_index] = address
    #         elif address < memLine_itr[beginAddress_index] and memLine_itr[beginAddress_index] - address < unitSize * 2:
    #             memLine_itr[frequency_index] += 1
    #             memLine_itr[memorySize_index] += unitSize
    #             memLine_itr[beginAddress_index] = address
    #         else:
    #             memoryLine[frequency_index] = 1
    #             memoryLine[beginAddress_index] = address
    #             memoryLine[endAddress_index] = address + unitSize
    #             memoryLine[memorySize_index] = unitSize
    #             memoryLine[index_index] = total_number + 1
    #             total_number = memoryLine[index_index]
    #             memoryGroup.append(memoryLine)
    # else:
    #     memoryLine[frequency_index] = 1
    #     memoryLine[beginAddress_index] = address
    #     memoryLine[endAddress_index] = address + unitSize
    #     memoryLine[memorySize_index] = unitSize
    #     memoryLine[index_index] = total_number + 1
    #     total_number = memoryLine[index_index]
    #     memoryGroup.append(memoryLine)

with open('eggs.csv', 'w', newline='') as csvFile:
    memWriter = csv.writer(csvFile, delimiter=' ', quotechar='|', quoting=csv.QUOTE_MINIMAL)
    memWriter.writerow(['Spam'] * 5 + ['Baked Beans'])
    memWriter.writerow(['Spam', 'Lovely Spam', 'Wonderful Spam'])
