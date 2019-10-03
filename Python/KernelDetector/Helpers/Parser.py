#!/usr/bin/python
# -*- coding: utf-8 -*-
from Classes.Op import Op
from Classes.ConvOp import ConvOp
from Classes.AllocaOp import AllocaOp
from Classes.StoreOp import StoreOp
from Classes.LoadOp import LoadOp
from Classes.AluOp import AluOp
from Classes.AshrOp import AshrOp
from Classes.LshrOp import LshrOp
from Classes.OrOp import OrOp
from Classes.SdivOp import SdivOp
from Classes.AddOp import AddOp
from Classes.SubOp import SubOp
from Classes.MulOp import MulOp
from Classes.UdivOp import UdivOp
from Classes.SremOp import SremOp
from Classes.UremOp import UremOp
from Classes.AndOp import AndOp
from Classes.ShlOp import ShlOp
from Classes.truncToOp import truncToOp
from Classes.zextToOp import zextToOp
from Classes.sextToOp import sextToOp
from Classes.fpTruncOp import fpTruncOp
from Classes.uitofp import uitofp
from Classes.sitofp import sitofp
from Classes.ptrtoint import ptrtoint
from Classes.inttoptr import inttoptr
from Classes.fptoui import fptoui
from Classes.fptosi import fptosi
from Classes.fpext import fpext
from Classes.bitcast import bitcast
from Classes.icmpOp import icmpOp
from Classes.fcmpOp import fcmpOp
import zlib

def ParserFile(file, compressed):
    if compressed == True:
        stream = open(file, 'rb')
        text = str(zlib.decompress(stream.read()).decode('utf-8'))
    else:
        stream = open(file, 'r')
        text = stream.read()
    lines = text.split('\n')
    version = ''
    ops = []
    for j in range(len(lines)):
        line = lines[j]
        if line[:2] != "  ": #its a label
            continue
        address = None
        ultimaValue = None
        archValue = None
        typeToValue = None
        condition = None
        sepperated = line.split(';')
        inst = sepperated[0]
        if len(sepperated) > 1:
            for i in range(1, len(sepperated)):
                split = sepperated[i].split(':')
                if split[0] == 'version':
                    if version != '':
                        raise Exception('Version tag duplicated')
                    else:
                        version = split[1]
                elif split[0] == 'line':
                    try:
                        line = int(split[1])
                    except ValueError:
                        raise Exception('Line attribute on line ' + str(j) + ' is not an integer')
                elif split[0] == 'block':
                    try:
                        block = int(split[1])
                    except ValueError:
                        raise Exception('Block attribute on line ' + str(j) + ' is not an integer')
                elif split[0] == 'address':
                    try:
                        address = int(split[1])
                    except ValueError:
                        raise Exception('Address attribute on line ' + str(j) + ' is not an integer')
                else:
                    raise Exception('Unrecognized tag: ' + split[0])
        if inst != '':
            metaSplit = inst.split('!')
            rawExpr = metaSplit[0].strip().strip(',').strip()

            # we ignore other metadata at this point, can be changed
            # instructions found here: http://llvm.org/docs/LangRef.html
            
            if rawExpr.startswith('%'):
                instStplit = rawExpr.split('=')
                code = instStplit[1].strip().split(' ')[0]
                ops.append(GetOp(line,
                    block,
                    rawExpr,
                    code,
                    ultimaValue,
                    archValue,
                    condition,
                    typeToValue,
                    address,))
            else:
                
                instSplit = rawExpr.split(' ')
                code = instSplit[0].strip()
                ops.append(GetOp(line,
                    block,
                    rawExpr,
                    code,
                    ultimaValue,
                    archValue,
                    condition,
		    typeToValue,
                    address,))
    return ops


def GetOp(line,
    block,
    expr,
    code,
    ultimaValue,
    archValue,
    condition,
    typeToValue,
    address=None,):
    if code == 'alloca':
        return AllocaOp(line, block, expr)
    elif code == 'store':
        return StoreOp(line, block, expr, address)
    elif code == 'load':
        return LoadOp(line, block, expr, address)
    #elif code == 'mul':
     #   return MulOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'add':
        #return AddOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'sub':
     #   return SubOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'and':
        #return AndOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'or':
        #return OrOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'xor':
        #return XorOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'shl':
        #return ShlOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'ashr':
        #return AshrOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'lshr':
        #return LshrOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'sdiv':
        #return SdivOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'udiv':
        #return UdivOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'srem':
        #eturn SremOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'urem':
        #return UremOp(line, block, expr, ultimaValue, archValue)
    #elif code == 'trunc':
        #return truncToOp(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'zext':
        #return zextToOp(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'sext':
        #return sextToOp(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'fptrunc':
        #return fpTruncOp(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'uitofp':
        #return uitofp(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'sitofp':
        #return sitofp(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'ptrtoint':
        #return ptrtoint(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'inttoptr':
        #return inttoptr(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'fptoui':
        #return fptoui(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'fptosi':
        #return fptosi(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'fpext':
        #return fpext(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'bitcast':
        #return Op(line, block)
        #return bitcast(line, block, expr, ultimaValue, typeToValue)
    #elif code == 'icmp':
     #   return icmpOp(line, block, expr, ultimaValue, archValue, condition)
    #elif code == 'fcmp':
     #   return fcmpOp(line, block, expr, ultimaValue, archValue, condition)
    else:
        return Op(line, block)
        #raise Exception('LLVM op code not recognized: ' + code)
