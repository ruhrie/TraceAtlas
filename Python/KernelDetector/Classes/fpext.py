#!/usr/bin/python
# -*- coding: utf-8 -*-

# from Classes.Op import Op

from Classes.ConvOp import ConvOp


class fpext(ConvOp):

    def __init__(
        self,
        line,
        block,
        inst,
        ultimaValue,
        typeToValue
        ):

        ConvOp.__init__(
            self,
            line,
            block,
            inst,
            ultimaValue,
            typeToValue
            )
        eqSplit = inst.split('=')
        self.output = eqSplit[0].strip()
        toSeparated = eqSplit[1].split('to')
        self.typeToValue = toSeparated[1].strip(' ')
        if len(toSeparated) == 2:
            spaceSeparated = toSeparated[0].strip().split(' ')
            if len(spaceSeparated) == 3:
                self.type = spaceSeparated[1]
                self.ultimaValue = spaceSeparated[2]            
            else:
                raise Exception('Illegally formatted fpext expression: '
                                 + inst)
        else:
            raise Exception('Illegally formatted fpext expression: '
+ inst)
