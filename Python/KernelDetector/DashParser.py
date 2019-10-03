import argparse
import Helpers.Parser as Parser
import Helpers.Hotcode as hc
arg_parser = argparse.ArgumentParser()
arg_parser.add_argument("-i", "--input_file", default="raw.trace", help="Trace file to be read")
arg_parser.add_argument("-t", "--threshhold", default=0.90, help="The threshhold of code coverage required in the program")
arg_parser.add_argument("-f", "--output_file", default="kernels.csv", help="The output file")
arg_parser.add_argument("-c", "--compressed", default=False, help="Is the input compressed")

threshhold = 0.99

def main(file, outputFile, compressed):
    ops = Parser.ParserFile(file, compressed)
    hc.detectHotcodeEdges(ops, threshhold, outputFile)


if __name__ == "__main__":
    args = arg_parser.parse_args()
    threshhold = args.threshhold
    if args.compressed == "False":
        comp = False
    else:
        comp = True
    main(args.input_file, args.output_file, comp)

    