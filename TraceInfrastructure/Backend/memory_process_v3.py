def firstStore(addr, t):
    # end kernel if none at last
    if t > 0:
        virAddr[str(addr) + '-' + str(t)] = [addr, t, []]
        ## todo new coming repeat address will replace old one here
        if addr not in deAlias:
            deAlias[addr] = str(addr) + '-' + str(t)  # latest store
        elif not virAddr[deAlias[addr]][2]:
            virAddr[deAlias[addr]][2].append(t)
            deAlias[addr] = str(addr) + '-' + str(t)
        else:
            deAlias[addr] = str(addr) + '-' + str(t)
    else:
        ## todo repeat addr here
        if addr not in inputList:
            inputList.append(addr)


def livingLoad(addr, t):
    virAddr[deAlias[addr]][2].append(t)


inputList = []
outputList = []
virAddr = {}
determineDir = {}
aliveTbl = []
deadTbl = []
deAlias = {}
file = open('fir_raw.trc', 'rt')
address = 0
timing = -1
output = []
inputWorking = []
for line in file.readlines():
    timing += 1
    line = line.split(':')
    try:
        address = int(line[1])
    except:
        print("wrong split at line number", timing)
        continue

    if address in deAlias and line[0] == "LoadAddress":
        livingLoad(address, timing)
    elif line[0] == "LoadAddress":
        # start kernel
        firstStore(address, -1)

    elif line[0] == "StoreAddress":
        firstStore(address, timing)

    inputWorking.append(inputList.__len__())
for time in range(0, timing):
    timeline = []
    for addrPair in virAddr:
        if virAddr[addrPair][2]:
            if int(virAddr[addrPair][1]) < time < int(virAddr[addrPair][2][-1]):
                timeline.append(addrPair)
    aliveTbl.append(timeline)
    print("time progress:", time)
    output.append(timeline.__len__())

f = open("./workingSet.txt", "w")
for line in output:
    f.writelines(str(line) + '\n')
f.close()

f = open("./inputWorkingSet.txt", "w")
for line in inputWorking:
    f.writelines(str(line) + '\n')
f.close()
