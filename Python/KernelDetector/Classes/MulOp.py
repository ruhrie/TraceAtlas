#!/usr/bin/python
# -*- coding: utf-8 -*-
from Classes.AluOp import AluOp 

class MulOp(AluOp):

    def __init__(
        self,
        line,
        block,
        inst,
	ultimaValue,
        archValue,
        ):

        AluOp.__init__(
            self,
            line,
            block,
            inst,
            ultimaValue,
            archValue,
            )
        eqSplit = inst.split('=')
        self.output = eqSplit[0].strip().strip('<').strip('>')
        commaSeparated = eqSplit[1].split(',')
        if len(commaSeparated) == 2:
            spaceSeparated = commaSeparated[0].strip().split(' ')
            if len(spaceSeparated) == 3:
                self.type = spaceSeparated[1]
                self.ultimaValue = spaceSeparated[2]
                self.archValue = commaSeparated[1].strip()
                       
            elif len(spaceSeparated) == 4:
                self.wrap1 = spaceSeparated[1]
                self.type = spaceSeparated[2]
                self.ultimaValue = spaceSeparated[3]
                self.archValue = commaSeparated[1]
                               
            elif len(spaceSeparated) == 5:
                self.wrap1 = spaceSeparated[1]
                self.wrap2 = spaceSeparated[2]
                self.type = spaceSeparated[3]
                self.ultimaValue = spaceSeparated[4]
                self.archValue = commaSeparated[1]

            else:
                raise Exception('Illegally formatted MulOp expression: '
                                 + inst)
        else:
            raise Exception('Illegally formatted MulOp expression: '
                            + inst)




