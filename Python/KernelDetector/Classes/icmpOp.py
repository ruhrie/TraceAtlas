from Classes.ConditionalOp import ConditionalOp

class icmpOp(ConditionalOp):
      def __init__(
        self,
        line,
        block,
        inst,
	ultimaValue,
        archValue,
        condition,
        ):

        ConditionalOp.__init__(
            self,
            line,
            block,
            inst,
            ultimaValue,
            archValue,
            condition,
            )

        eqSplit = inst.split('=')
        self.target = eqSplit[0]
        commaSplit = eqSplit[1].split(',')
        self.ultimaValue = commaSplit[1]
        if(len(commaSplit) != 2):
               raise Exception('Illegally formed icmp expression:' + inst)
        else: 
               condList = ['eq', 'ne', 'ugt', 'uge', 'ult', 'ule', 'sgt', 'sge', 'slt', 'sle']
               self.ultimaValue = commaSplit[1]
               spaceSplit = eqSplit[1].strip().split(' ')
               self.condition = spaceSplit[1]
               typ = spaceSplit[1].strip(' ')
               self.archValue = spaceSplit[2]
               if(typ not in condList):
                   raise Exception('Invalid condition. Illegally formed icmp expression: '+ inst)
               else:
                   self.type = typ  
