inputSize = 0


def firstStore(addr,t , op):
    # end kernel if none at last  str(t) + '-' + str(addr)   str(addr) + '-' + str(t)
    global inputSize
    if op > 0:
        virAddr[str(addr) + '-' + str(t)] = [addr,t,[]]
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
        if addr not in inputAddrList:
            inputAddrList.append(addr)
            inputList.append([addr, t, 0])
            inputSize += 1
        else:
            inputList.append([addr, t, 0])


def livingLoad(addr,t):
    virAddr[deAlias[addr]][2].append(t)


def myFunc(e):
    e = e.split('-')
    return int(e[1])

inputAddrList = []
inputList = []
outputList = []
virAddr = {}
aliveTbl = []
deadTbl = []
deAlias = {}
file = open('./trace/fft.trc','rt')
address = 0
timing = -1
output = []
inputWorking = []
outputWorking = []

for line in file.readlines():
    timing += 1
    line = line.split(':')
    try:
        address = int(line[1])
    except:
        print("wrong split at line number",timing)
        continue

    if address in deAlias and line[0] == "LoadAddress":
        livingLoad(address,timing)
    elif line[0] == "LoadAddress":
        # start kernel
        firstStore(address,timing, -1)

    elif line[0] == "StoreAddress":
        firstStore(address,timing, 1)
    # inputWorking.append(inputList.__len__())
# inputList.reverse()
# inputWorkingList = []
# for line in inputList:
#     if line not in inputWorkingList:
#         inputWorkingList.append(line)
intputLiveness = []
for i in reversed(inputList):
    if i[0] in intputLiveness:
        i[2] = 0 # alive
    else:
        intputLiveness.append(i[0])
        i[2] = 1 # dead
alivenumber = inputSize
breakflag = 0
for time in range(0, timing):
    for i in inputList:
        if i[1] <= time and i[2] == 1:
            alivenumber = alivenumber -1
            inputList.remove(i)
        elif i[1] < time and i[2] == 0:
            inputList.remove(i)
        elif i[1]> time:
            breakflag = 1
            inputWorking.append(alivenumber)
            break
    if breakflag == 0:
        inputWorking.append(alivenumber)
    if inputList.__len__() == 0:
        inputWorking.append(0)

L = list(virAddr.keys())
L.sort(key=myFunc)

for time in range(0,timing):
    timeline = []
    reviseDic = []
    for addrPair in L:
        if virAddr[addrPair][1] > time:
            break
        if virAddr[addrPair][2]:
            if int(virAddr[addrPair][1]) < time < int(virAddr[addrPair][2][-1]):
                timeline.append(addrPair)
            elif time > int(virAddr[addrPair][2][-1]):
                reviseDic.append(addrPair)
        else:
            if addrPair not in outputList:
                outputList.append(addrPair)
    for i in reviseDic:
        del virAddr[i]
        L.remove(i)
    aliveTbl.append(timeline)
    outputWorking.append(outputList.__len__())
    if time%100 == 0:
        print("time progress:",time)
    output.append(timeline.__len__())

f = open("./workingSet.txt","w")
for line in output:
    f.writelines(str(line) + '\n')
f.close()

f = open("./inputWorkingSet.txt","w")
for line in inputWorking:
    f.writelines(str(line) + '\n')
f.close()

f = open("./OutputWorkingSet.txt","w")
for line in outputWorking:
    f.writelines(str(line) + '\n')
f.close()
