inputSize = 0


def firstStore(addr,t,op):
    # end kernel if none at last  str(t) + '-' + str(addr)   str(addr) + '-' + str(t)
    global inputSize
    if op > 0:
        virAddr[str(addr) + '@' + str(t)] = [addr,t,[]]
        ## todo new coming repeat address will replace old one here
        if addr not in deAlias:
            deAlias[addr] = str(addr) + '@' + str(t)  # latest store
        elif not virAddr[deAlias[addr]][2]:
            virAddr[deAlias[addr]][2].append(t)
            deAlias[addr] = str(addr) + '@' + str(t)
        else:
            deAlias[addr] = str(addr) + '@' + str(t)
    else:
        ## todo repeat addr here
        if addr not in deAliasInput:
            inputSize += 1
            deAliasInput[addr] = str(addr) + '@' + str(t)  # latest store
            virAddrInput[str(addr) + '@' + str(t)] = [addr,[t]]
        else:
            virAddrInput[deAliasInput[addr]][1].append(t)


def livingLoad(addr,t,op):
    if op ==1:
        virAddr[deAlias[addr]][2].append(t)
    else:
        virAddrInput[deAliasInput[addr]][1].append(t)


def myFunc(e):
    e = e.split('@')
    return int(e[1])

inputAddrList = []
inputList = []
outputList = []
virAddr = {}
virAddrInput = {}
aliveTbl = []
deadTbl = []
deAlias = {}
deAliasInput = {}
file = open('./trace/fir-input-kernel.trc','rt')
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
        livingLoad(address,timing, 1)
    elif address in deAliasInput and line[0] == "LoadAddress":
        livingLoad(address,timing, -1)

    elif line[0] == "LoadAddress":
        # start kernel
        firstStore(address,timing, -1)

    elif line[0] == "StoreAddress":
        firstStore(address,timing,1)

L = list(virAddr.keys())
L.sort(key=myFunc)
Lin = list(virAddrInput.keys())
Lin.sort(key=myFunc)
alivenumber = inputSize
for time in range(0, timing+1):
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
    output.append(timeline.__len__())

    timeline = []
    reviseDic = []
    for addrPair in Lin:
        if virAddrInput[addrPair][1][0] > time:
            break
        if time > int(virAddrInput[addrPair][1][-1]):
            reviseDic.append(addrPair)
    if Lin.__len__():
        alivenumber = alivenumber - reviseDic.__len__()
    else:
        alivenumber = 0
    inputWorking.append(alivenumber)
    for i in reviseDic:
        del virAddrInput[i]
        Lin.remove(i)
    if time%100 == 0:
        print("time progress:",time)

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
