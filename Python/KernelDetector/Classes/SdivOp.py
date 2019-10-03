#!/usr/bin/python
# -*- coding: utf-8 -*-
from Classes.AluOp import AluOp


class SdivOp(AluOp):

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
            spaceSeparated = commaSeparated[0].split(' ')
            if len(spaceSeparated) == 3:
                self.type = spaceSeparated[1]
                self.ultimaValue = spaceSeparated[2]
                if len(commaSeparated[1].strip(' ')) == 1:
                    self.archValue = commaSeparated[1]
                else:
                    raise Exception('Illegally formatted SdivOp expression: '
                                     + inst)
            elif len(spaceSeparated) == 4:
                self.exact = spaceSeparated[1]
                self.type = spaceSeparated[2]
                self.ultimaValue = spaceSeparated[3]
                if len(commaSeparated[1].strip(' ')) == 1:
                    self.archValue = commaSeparated[1]
                else:
                    raise Exception('Illegally formatted SdivOp expression: '
                                     + inst)
            else:
                raise Exception('Illegally formatted SdivOp expression: '
                                 + inst)
        else:
            raise Exception('Illegally formatted SdivOp expression: '
                            + inst)

