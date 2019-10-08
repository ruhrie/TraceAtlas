from Classes.ConditionalOp import ConditionalOp

class fcmpOp(ConditionalOp):
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
               raise Exception('Illegally formed fcmp expression:' + inst)
        else: 
               condList = ['false', 'oeq', 'ogt', 'oge', 'olt', 'ole', 'one', 'ord', 'ueq', 'ugt', 'uge', 'ult', 'ule', 'une', 'uno', 'true']
               self.ultimaValue = commaSplit[1]
               spaceSplit = eqSplit[1].strip().split(' ')
               self.condition = spaceSplit[1]
               typ = spaceSplit[1].strip(' ')
               self.archValue = spaceSplit[2]
               if(typ in condList):
                   self.type = typ  
               else :
                   raise Exception('Invalid condition. Illegally formed fcmp expression: '+ inst)
                   
