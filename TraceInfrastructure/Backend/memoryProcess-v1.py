import csv


def TimingGroupAppend(timingGroup_i,instruction_i,address_i,timing_i):
    result_got = 0
    for linet in timingGroup_i:
        if linet[0] == address_i:
            result_got = 1
            lifeinner = linet[-1]
            if instruction_i == "LoadAddress":
                lifeinner += 1
                linet.append(lifeinner)
            elif lifeinner > 0:
                linet[1] += 1
                lifeinner -= 1
                linet.append(lifeinner)
    if result_got ==0:
        memoryLiveLine_i = [0]*(2+timing)
        memoryLiveLine_i[0] = address_i
        if line[0] == "LoadAddress":
            memoryLiveLine_i[timing+1] = 1
        timingGroup_i.append(memoryLiveLine_i)


timingGroup = []
file = open('raw.trc','rt')
address = 0
timing = 0

for line in file.readlines():
    life = 0
    timing += 1
    # addr/life_repeat_number/time_live_or_not
    memoryLiveLine = [0]*3
    line = line.split(':')
    address = 0
    try:
        address = int(line[1])
    except:
        print("wrong address %s",line[1])

    if timingGroup.__len__() > 0:
        TimingGroupAppend(timingGroup,line[0],address,timing)
    else:
        memoryLiveLine[0] = address
        if line[0] == "LoadAddress":
            memoryLiveLine[2] = 1
        timingGroup.append(memoryLiveLine)
    for line in timingGroup:
        if line.__len__()<timing + 2:
            line.append(line[-1])
all = 1



with open('eggs.csv','w',newline='') as csvFile:
    memWriter = csv.writer(csvFile,delimiter=' ',quotechar='|',quoting=csv.QUOTE_MINIMAL)
    memWriter.writerow(['Spam']*5 + ['Baked Beans'])
    memWriter.writerow(['Spam','Lovely Spam','Wonderful Spam'])
