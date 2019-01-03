#!/usr/bin/python
import sys
import os.path

def main():
	filename = sys.argv[1]
	file = open(filename)
	line = file.readline()
	while line:
		line = line.replace("\n","")
		cmd = "klee -json=\'"+line+"\' 2>&1 1>result.txt"
		
		os.system(cmd)
		# print(cmd)
		line = file.readline()

if __name__ =="__main__":
	main()
