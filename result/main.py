#!/usr/bin/python
import os.path
import sys


def main():
    filename = sys.argv[1]
    file = open(filename)
    line = file.readline()
    while line:
        line = line.replace(":\n", "")
        line = line.split(":")
        cmd = "llvm-link -o built-in.bc"
        for bc in line:
            cmd = cmd + " " + bc
        # print(cmd)
        os.system(cmd)
        line = file.readline()
        line = line.replace("\n", "")
        cmd = "time klee -json=\'" + line + "\' built-in.bc 2>&1 | tee >> confirm_result.log"
        os.system(cmd)
        # print(cmd)
        line = file.readline()


if __name__ == "__main__":
    main()
