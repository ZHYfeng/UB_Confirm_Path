#!/usr/bin/python
import sys
import os.path

def main():
	filename = sys.argv[1]
	file = open(filename)
	line = file.readline()
	while line:
		line = line.replace("\n","")
		cmd = "/home/yizhuo/2018_klee_confirm_path/build/bin/klee -json=\'"+line+"\' 2>&1 | tee >> confirm_result.log"
		
		os.system(cmd)
		# print(cmd)
		line = file.readline()

if __name__ =="__main__":
	main()
