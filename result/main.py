#!/usr/bin/python
import os.path
import sys
import psutil
import subprocess
import time
import multiprocessing
import signal

# usage
# python main_remote.py file.json

# out put
# time_out_file_name: all link files and jsons of time out
# memory_out_file_name: all link files and jsons of memory out
# klee_result_file_name: all jsons of klee
# klee_log_file_name: all log of klee. I do not mv those log into one file.

# those variables need you change

total_cpu = multiprocessing.cpu_count() - 8
klee_path = "/home/yhao/git/2018_klee_confirm_path/cmake-build-debug/bin/klee"
klee_log_file_name = "confirm_result.log"
klee_result_file_name = "confirm_result.json"

schedule_time = 1  # second
time_out = 30  # second
time_out_file_name = "time_out.json"

# notice: for the reason that python can not kill the klee quickly, it is better to set this small.
memory_out = 1 * 1024 * 1024 * 1024  # byte
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

        print(self.klee_cmd)
        self.p = subprocess.Popen(self.klee_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True, preexec_fn=os.setsid)
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
                print("time out : ")
                print(self.t1- self.t0)
                self.close()
                self.output_json(time_out_file_name)
                return self.check_execution_state()

            if self.max_vms_memory > memory_out:
                print("memory out : ")
                print(self.max_vms_memory)
                self.close()
                self.output_json(memory_out_file_name)
                return self.check_execution_state()

        except psutil.NoSuchProcess:
            return self.check_execution_state()

        return self.check_execution_state()

    def is_running(self):
        return psutil.pid_exists(self.p.pid) and self.p.poll() is None

    def output_json(self, file_name):
        f = open(self.path + "/" +file_name, "a")
        f.write(self.link_file)
        f.write(self.json + "\n")
        f.close()

    def check_execution_state(self):
        if not self.execution_state:
            return False
        if self.is_running():
            return True
        self.execution_state = False
        self.t1 = time.time()
        return False

    def close(self, kill=True):
        if self.initd == False:
            return

        try:
            pp = psutil.Process(self.p.pid)
            if kill:
                os.killpg(os.getpgid(self.p.pid), signal.SIGKILL)
                self.p.kill()
                pp.kill()
            else:
                os.killpg(os.getpgid(self.p.pid), signal.SIGTERM)
                self.p.terminate()
                pp.terminate()
        except psutil.NoSuchProcess:
            if self.p.returncode != right_return_code:
                self.output_json(klee_error_result_file_name)
            pass

        #self.output, self.err = self.p.communicate()
        #print(self.output)
        #print(self.err)

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
    check = True
    while check:
        check = False
        for i in range(total_cpu):
            if tasks[i].check_execution_state():
                check = True
                tasks[i].poll()
        time.sleep(schedule_time)

    for i in range(total_cpu):
        tasks[i].close()


def read_all_json(file_name):
    f = open(file_name, "a")
    for i in range(total_cpu):
        path_file_name = str(i) + "/" + file_name
        print(path_file_name)
        if os.path.isfile(path_file_name):
            tf = open(path_file_name, "r")
            f.write(tf.read())
            tf.close()
    f.close()


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
    json_index = 0
    line = file.readline()
    while line:
        json_index = json_index + 1
        link_file = line
        json = file.readline()
        index = run_next_json(index, link_file, json)
        print("json index : " + str(json_index) + " path : " + str(index))
        line = file.readline()

    wait_all_json()

    read_all_json(klee_result_file_name)
    read_all_json(time_out_file_name)
    read_all_json(memory_out_file_name)
    read_all_json(klee_error_result_file_name)


if __name__ == "__main__":
    main()
