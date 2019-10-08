#!/usr/bin/python
# -*- coding: utf-8 -*-
from Classes.Op import Op 
import re

class phiOp(Op):

    def __init__(
        self,
        line,
        block,
        inst,
        ):

        Op.__init__(
            self,
            line,
            block,
            )
        eqsplit = inst.split('=')
        self.target = eqsplit[0]
        spacesplit = eqsplit[1].strip().split(' ')
        self.type = spacesplit[1]
        phivectors = re.findall(r'\[([^]]*)\]',eqsplit[1])
        l = len(phivectors)
        for i in range(0,l):
            s = phivectors[i]
            s = s.split(",")
            if(len(s) != 2):
                  raise Exception('Illegally formatted phiOp  expression: ' + inst)
            else:      
                  print("Type : "+s[0])
                  print("Label : "+s[1])
