#!/usr/bin/python
import os.path
import sys
import psutil
import subprocess
import shutil
import threading
import time

root_path = "./"
total_cpu = 6
klee_path = "/home/yhao/git/2018_klee_confirm_path/build/bin/klee"
schedule_time = 10

class ProcessTimer:
    def __init__(self):
        self.execution_state = False

    def init(self, path, link_file, json):
        self.path = path
        # link the given bitcode files
        bc_list = link_file.replace(":\n", "")
        bc_list = bc_list.split(":")
        link_cmd = "llvm-link -o " + "./built-in.bc"
        for bc in bc_list:
            link_cmd = link_cmd + " " + bc
        self.link_cmd = link_cmd

        json = json.replace("\n", "")
        klee_cmd = klee_path + " -json=\'" + json + "\' " + "./built-in.bc 2>&1 | tee >> confirm_result.log"
        self.klee_cmd = klee_cmd
        self.execution_state = False

    def execute(self):
        self.max_vms_memory = 0
        self.max_rss_memory = 0
        self.t1 = None
        self.t0 = time.time()

        if not os.path.exists(self.path):
            os.makedirs(self.path)
        os.chdir(self.path)

        link_subprocess = subprocess.Popen(self.link_cmd, shell=True)
        link_subprocess.wait()

        self.p = subprocess.Popen(self.klee_cmd, shell=True)
        self.execution_state = True

        os.chdir("../")

    def poll(self):
        if not self.check_execution_state():
            return False
        self.t1 = time.time()
        try:
            pp = psutil.Process(self.p.pid)

            # obtain a list of the subprocess and all its descendants
            descendants = list(pp.children(recursive=True))
            descendants = descendants + [pp]

            rss_memory = 0
            vms_memory = 0

            # calculate and sum up the memory of the subprocess and all its descendants
            for descendant in descendants:
                try:
                    mem_info = descendant.memory_info()

                    rss_memory += mem_info[0]
                    vms_memory += mem_info[1]
                except psutil.NoSuchProcess:
                    # sometimes a subprocess descendant will have terminated between the time
                    # we obtain a list of descendants, and the time we actually poll this
                    # descendant's memory usage.
                    pass
            self.max_vms_memory = max(self.max_vms_memory, vms_memory)
            self.max_rss_memory = max(self.max_rss_memory, rss_memory)

        except psutil.NoSuchProcess:
            return self.check_execution_state()

        if self.t1 -self.t0 > 1800:
            self.close()

        print('return code:', self.p.returncode)
        print('time:', self.t1 - self.t0)
        print('max_vms_memory:', self.max_vms_memory)
        print('max_rss_memory:', self.max_rss_memory)

        return self.check_execution_state()

    def is_running(self):
        return psutil.pid_exists(self.p.pid) and self.p.poll() == None

    def check_execution_state(self):
        if not self.execution_state:
            return False
        if self.is_running():
            return True
        self.executation_state = False
        self.t1 = time.time()
        return False

    def close(self, kill=False):
        try:
            pp = psutil.Process(self.p.pid)
            if kill:
                pp.kill()
            else:
                pp.terminate()
        except psutil.NoSuchProcess:
            pass

        if not os.path.exists(self.path):
            os.makedirs(self.path)
        os.chdir(self.path)
        rm_cmd = "rm -rf klee-*"
        rm_subprocess = subprocess.Popen(rm_cmd, shell=True)
        rm_subprocess.wait()
        os.chdir("../")


tasks = [ProcessTimer() for i in range(total_cpu)]

def run_next_json(index, link_file, json):
    while tasks[index].check_execution_state():
        tasks[index].poll()
        index = index + 1
        if index >= total_cpu:
            index = index - total_cpu
            time.sleep(schedule_time)

    tasks[index].close()
    path = str(index)
    tasks[index].init(path, link_file, json)
    tasks[index].execute()
    return index

def main():
    filename = sys.argv[1]
    file = open(filename)

    index = 0
    line = file.readline()
    while line:
        link_file = file.readline()
        json = file.readline()
        index = run_next_json(index, link_file, json)

if __name__ == "__main__":
    main()
