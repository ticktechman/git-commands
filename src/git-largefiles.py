#!/usr/bin/env python3
###############################################################################
##
##       filename: git-largefiles.py
##    description: find top N large files in current repo
##        created: 2023/09/28
##         author: ticktechman
##
###############################################################################
import os
import re
import subprocess
import argparse

## run a git command
def git_command(command, line_parser):
    try:
        # Start the Git process
        process = subprocess.Popen(
            command,
            shell=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        # Process the standard output line by line
        for line in process.stdout:
            line = line.strip()  # Remove trailing newline and whitespace
            line_parser(line)

        # Wait for the command to finish
        process.wait()

        # Check if the command executed successfully (return code 0 indicates success)
        if process.returncode != 0:
            # Error output
            stderr_result = process.stderr
            print("Error output:")
            print(stderr_result)

    except Exception as e:
        print("An exception occurred:", e)

def git_dir():
    cmd = "git rev-parse --show-toplevel"
    gitdir = ""
    def set_dir(line):
        nonlocal gitdir
        gitdir = line
    git_command(cmd, set_dir)
    return gitdir

class record:
    def __init__(self, size=0, fn=""):
        self.size = size
        self.fn = fn
class LargeFiles:
    def __init__(self):
        self.err = 0
        self.count = 0
        self.limit = 0
        self.fn_list = ".largefiles.list.tmp"
        self.meta = dict()

    def top(self, n=10):
        self.count = n;
        return self

    def size(self, sz):
        self.limit = sz
        return self
    def run(self):
        self.meta = dict()
        self._list()
        self._filter()
        os.remove(self.fn_list)
        return self

    def _list(self):
        if self.err:
            return self
        gitdir = git_dir()
        cmd = "git verify-pack -v {}/.git/objects/pack/*.idx | grep blob | sort -k 3 -nr > {}".format(gitdir, self.fn_list)
        self.err = os.system(cmd)
        return self
    def _filter(self):
        idx = 0
        count = self.count if self.count > 0 else 65535
        ordered_oid = []

        for line in open(self.fn_list, "r"):
            values = re.split(r'\s+', line, 3)
            oid = values[0]
            size = int(values[2])
            idx += 1
            if idx > count or size < self.limit:
                break
            ordered_oid.append(oid)
            self.meta[oid] = record(size)

        def parser(line):
            val = re.split(r'\s+', line, 2)
            if self.meta.get(val[0]):
                self.meta[val[0]].fn = val[1]
        cmd = 'git rev-list --objects --all'
        git_command(cmd, parser)

        for one in ordered_oid:
            rec = self.meta[one]
            print("{} {:>10d} {}".format(one,  rec.size, rec.fn))

def parse_size(input_string):
    ## multipliers
    multipliers = {
        'k': 1024,
        'm': 1024 * 1024,
        'g': 1024 * 1024 * 1024
    }

    # ignore case
    input_string = input_string.lower()

    number_str = ''
    char_str = ''

    for char in input_string:
        if char.isdigit() or char == '.':
            number_str += char
        elif char in multipliers:
            char_str += char

    # change to float type
    number = float(number_str) if number_str else 0.0

    # calculate
    if char_str:
        multiplier = multipliers.get(char_str[-1], 1)  # default 1 means no char string
        result = number * multiplier
    else:
        result = number

    return result


def main():
    desc = '''
    find the top N largest files.
    example:
        git largefiles -t 10 -s 1M  # find top 10 files size >= 10MB
        git largefiles -s 100K      # find all files size >= 100KB
    '''
    parser = argparse.ArgumentParser(desc)
    parser.add_argument("-t", "--top", type=int, help="top n files")
    parser.add_argument("-s", "--size", type=str, help="size limit - 10=10B, 10K=10240B 1M=1024*1024B")
    args = parser.parse_args()

    lf = LargeFiles()

    if args.top is not None:
        lf.top(args.top)

    if args.size is not None:
        lf.size(int(parse_size(args.size)))

    lf.run()
if __name__ == "__main__":
    main()

###############################################################################
