#!/usr/bin/python
import os.path
import sys
import psutil
import subprocess
import time

# usage
# python main_remote.py file.json

# out put
# time_out_file_name: all link files and jsons of time out
# memory_out_file_name: all link files and jsons of memory out
# klee_result_file_name: all jsons of klee
# klee_log_file_name: all log of klee. I do not mv those log into one file.
# klee_error_result_file_name: all json with error code

# those variables need you change
total_cpu = 6
klee_path = "/home/yhao/git/2018_klee_confirm_path/cmake-build-debug/bin/klee"
klee_log_file_name = "confirm_result.log"
klee_result_file_name = "confirm_result.json"

schedule_time = 10  # second
time_out = 1800  # second
time_out_file_name = "time_out.json"

memory_out = 16 * 1024 * 1024 * 1024  # byte
memory_out_file_name = "memory_out.json"

right_return_code = 0
klee_error_result_file_name = "error.json"

# if you need change the path in link file
linux_kernel_path_in_json = "/home"
linux_kernel_path_in_this_pc = "/home"

klee_right_result = "KLEE: done: generated tests ="


class ProcessTimer:
    def __init__(self):
        self.initd = False
        self.execution_state = False

    def init(self, path, link_file, json):
        self.initd = True
        self.path = path
        # link the given bitcode files
        self.link_file = link_file
        self.link_file = self.link_file.replace(linux_kernel_path_in_json, linux_kernel_path_in_this_pc)
        bc_list = self.link_file.replace(":\n", "")
        bc_list = bc_list.split(":")
        link_cmd = "llvm-link -o " + "./built-in.bc"
        for bc in bc_list:
            link_cmd = link_cmd + " " + bc
        self.link_cmd = link_cmd

        self.json = json
        self.json = self.json.replace("\n", "")
        # klee_cmd = klee_path + " -json=\'" + self.json + "\' " + "./built-in.bc 2>&1 | tee >> " + klee_log_file_name
        klee_cmd = klee_path + " -json=\'" + self.json + "\' " + "./built-in.bc"
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

        self.p = subprocess.Popen(self.klee_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
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

            if self.t1 - self.t0 > time_out:
                self.output_time_out_json()
                self.close()

            if self.max_rss_memory > memory_out:
                self.output_memory_out_json()
                self.close()

        except psutil.NoSuchProcess:
            return self.check_execution_state()

        return self.check_execution_state()

    def is_running(self):
        return psutil.pid_exists(self.p.pid) and self.p.poll() == None

    def output_time_out_json(self):
        out_cmd = "echo " + self.link_file + " >> ./" + time_out_file_name
        out_subprocess = subprocess.Popen(out_cmd, shell=True)
        out_subprocess.wait()
        out_cmd = "echo " + self.json + " >> ./" + time_out_file_name
        out_subprocess = subprocess.Popen(out_cmd, shell=True)
        out_subprocess.wait()

    def output_memory_out_json(self):
        out_cmd = "echo " + self.link_file + " >> ./" + memory_out_file_name
        out_subprocess = subprocess.Popen(out_cmd, shell=True)
        out_subprocess.wait()
        out_cmd = "echo " + self.json + " >> ./" + memory_out_file_name
        out_subprocess = subprocess.Popen(out_cmd, shell=True)
        out_subprocess.wait()

    def output_error_json(self):
        out_cmd = "echo " + self.link_file + " >> ./" + klee_error_result_file_name
        out_subprocess = subprocess.Popen(out_cmd, shell=True)
        out_subprocess.wait()
        out_cmd = "echo " + self.json + " >> ./" + klee_error_result_file_name
        out_subprocess = subprocess.Popen(out_cmd, shell=True)
        out_subprocess.wait()

    def check_execution_state(self):
        if not self.execution_state:
            return False
        if self.is_running():
            return True
        self.executation_state = False
        self.t1 = time.time()
        return False

    def close(self, kill=False):
        if self.initd == False:
            return

        try:
            pp = psutil.Process(self.p.pid)
            if kill:
                pp.kill()
            else:
                pp.terminate()
        except psutil.NoSuchProcess:
            if self.p.returncode != right_return_code:
                self.output_error_json()
            pass

        self.output, self.err = self.p.communicate()

        if not os.path.exists(self.path):
            os.makedirs(self.path)
        os.chdir(self.path)
        rm_cmd = "rm -rf klee-*"
        rm_subprocess = subprocess.Popen(rm_cmd, shell=True)
        rm_subprocess.wait()
        os.chdir("../")

        self.initd = False


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

def wait_all_json():
    for i in range(total_cpu):
        if tasks[i].check_execution_state():
            tasks[i].p.wait()
            tasks[i].close()

def main():
    for i in range(total_cpu):
        rm_cmd = "rm -rf ./" + str(i)
        rm_subprocess = subprocess.Popen(rm_cmd, shell=True)
        rm_subprocess.wait()

    rm_cmd = "rm -rf ./" + klee_result_file_name + " ./" + time_out_file_name
    rm_cmd = rm_cmd + " ./" + memory_out_file_name + " ./" + klee_error_result_file_name
    rm_subprocess = subprocess.Popen(rm_cmd, shell=True)
    rm_subprocess.wait()

    filename = sys.argv[1]
    file = open(filename)

    index = 0
    line = file.readline()
    while line:
        link_file = line
        json = file.readline()
        index = run_next_json(index, link_file, json)
        line = file.readline()

    wait_all_json()

    for i in range(total_cpu):
        cat_cmd = "cat ./" + str(i) + "/confirm_result.json >> ./" + klee_result_file_name
        cat_subprocess = subprocess.Popen(cat_cmd, shell=True)
        cat_subprocess.wait()
        cat_cmd = "cat ./" + str(i) + "/" + time_out_file_name + " >> ./" + time_out_file_name
        cat_subprocess = subprocess.Popen(cat_cmd, shell=True)
        cat_subprocess.wait()
        cat_cmd = "cat ./" + str(i) + "/" + memory_out_file_name + " >> ./" + memory_out_file_name
        cat_subprocess = subprocess.Popen(cat_cmd, shell=True)
        cat_subprocess.wait()
        cat_cmd = "cat ./" + str(i) + "/" + klee_error_result_file_name + " >> ./" + klee_error_result_file_name
        cat_subprocess = subprocess.Popen(cat_cmd, shell=True)
        cat_subprocess.wait()


if __name__ == "__main__":
    main()
