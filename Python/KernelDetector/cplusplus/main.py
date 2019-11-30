simport argparse
import json
from EncodeDetect import DetectKernels
from EncodeExtractor import ExtractKernels
from EncodeDag import GenerateDag
arg_parser = argparse.ArgumentParser()

arg_parser.add_argument("-i", "--input_file", default="raw.trc", help="Trace file to be read", required=True)
arg_parser.add_argument("-t", "--threshhold", default=0.95, type=float, help="The threshhold of code coverage required in the program")
arg_parser.add_argument("-ht", "--hot_threshhold", default=512, type=int, help="Minimum instance count for kernel")
arg_parser.add_argument("-k", "--kernel_file", default=None, help="The kernel file")
arg_parser.add_argument("-j", "--kernel_two_file", default="kernel.json", help="The output kernel file")
arg_parser.add_argument("-c", "--csv_file", default=None, help="The intermediate csv file. Mandatory if initial stage > 0")
arg_parser.add_argument("-is", "--initial_stage", default = 0, type=int, help="The stage at which to start the flow")
arg_parser.add_argument("-fs", "--final_stage", default = 2, type=int, help="The stage at which to end the flow")
arg_parser.add_argument("-d", "--dag_file", default = None, help="The dag file")
arg_parser.add_argument("-n", "--new_line", action="store_true", help="Use new line feeds in progress bar")
args = arg_parser.parse_args()

def main():
    typeOneKernels = None
    typeTwoKernels = None
    if args.initial_stage <= 0:
        typeOneKernels = DetectKernels(args.input_file, args.threshhold, args.hot_threshhold, args.new_line)
        if args.csv_file != None:
            with open(args.csv_file, "w") as fp:
                for row in typeOneKernels:
                    localRow = list(row)
                    for i in range(len(localRow)):
                        entry = localRow[i]
                        fp.write(str(entry))
                        if i != len(localRow) - 1:
                            fp.write(",")
                    fp.write("\n")

    if args.initial_stage <= 1 and args.final_stage >= 1:
        if typeOneKernels == None:
            typeOneKernels = []
            with open(args.csv_file, "r") as fp:
                test = fp.read()
                spl = test.split("\n")
                for line in spl:
                    kern = set()
                    csl = line.split(",")
                    for entry in csl:
                        if entry.isnumeric():
                            kern.add(int(entry,0))
                    typeOneKernels.append(kern)
        typeTwoKernls = ExtractKernels(args.input_file, typeOneKernels, args.new_line)
        if args.kernel_two_file != None:
            with open(args.kernel_two_file, "w") as df:
                json.dump(typeTwoKernls, df)
    if args.initial_stage <= 2 and args.final_stage >= 2:
        if typeTwoKernels == None:
            typeTwoKernels = json.loads(open(args.kernel_two_file).read())
        dagData = GenerateDag(args.input_file, typeTwoKernels, args.new_line)

if __name__ == "__main__":
    main()