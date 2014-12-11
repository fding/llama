import argparse
import math
import re

class Experiment(object):
    def __init__(self, commit, params):
        self.commit = commit
        self.params = dict([
            tuple(kv.split('=')) for kv in params.split(',')
        ])
        self.outputs = []
        self.runtimes = []

class Results(dict):
    def __init__(self, *args, **kwargs):
        super(Results, self).__init__(*args, **kwargs)

class Runtimes(list):
    def __init__(self, *args, **kwargs):
        super(Runtimes, self).__init__(*args, **kwargs)

class LogReader(object):
    def __init__(self, fname):
        with open(fname, 'r') as f:
            self.lines = f.read().split('\n')
            self.current_line = 0

    def wait_for_line(self, pattern):
        p = re.compile(pattern)
        n = len(self.lines)
        i = self.current_line
        while i < n and not p.match(self.lines[i].strip()):
            i = i + 1
        return i

    def parse_trial(self, max_i):
        output = Results()
        runtimes = Runtimes()
        n = len(self.lines)
        j = self.wait_for_line('(NO_MADVISE)|(WITH_MADVISE)')
        if j >= max_i:
            return None
        else:
            self.current_line = j

        if self.lines[self.current_line].strip() == 'WITH_MADVISE':
            output.with_madvise = True
            runtimes.with_madvise = True
        else:
            output.with_madvise = False
            runtimes.with_madvise = False

        self.current_line = self.wait_for_line('==========BEFORE VM STAT==========')
        if self.current_line >= n: return None

        j = self.wait_for_line('==========LLAMA OUTPUT==========')
        if j >= n: return None

        j = self.wait_for_line('--------')
        if j >= n: return None
        self.current_line = j+1

        j = self.wait_for_line('--------')
        if j >= n: return None

        for line in self.lines[self.current_line: j]:
            runtimes.append(float(line.strip()))

        self.current_line = j

        for line in self.lines[self.current_line+1: j]:
            parts = line.split(':')
            if len(parts) == 2:
                output['before ' + parts[0].strip()] = parts[1].strip()

        self.current_line = j
        j = self.wait_for_line('==========AFTER VM STAT==========')
        if j >= n: return None

        for line in self.lines[self.current_line+1: j]:
            parts = line.split(':')
            if len(parts) == 2:
                output[parts[0].strip()] = parts[1].strip()
        self.current_line = j
        j = self.wait_for_line('==========END LLAMA OUTPUT==========')
        if j >= n: return None

        for line in self.lines[self.current_line+1: j]:
            parts = line.split(':')
            if len(parts) == 2:
                output['after ' + parts[0].strip()] = parts[1].strip()

        self.current_line = j+1

        return output, runtimes

    def parse(self):
        experiments = []
        n = len(self.lines)

        while self.current_line < n:
            self.current_line = self.wait_for_line('==========START EXPERIMENT==========')
            if self.current_line >= n:
                break

            a = Experiment(self.lines[self.current_line+1], 
                           self.lines[self.current_line+2])
            self.current_line += 3
            next_expt = self.wait_for_line('==========START EXPERIMENT==========')

            while True:
                if self.current_line >= next_expt:
                    break
                ans = self.parse_trial(next_expt)
                if ans:
                    output, runtimes = ans
                    a.outputs.append(output)
                    a.runtimes.append(runtimes)
                else:
                    break

            experiments.append(a)

        return experiments

def floatify(s):
    return float(s.split()[0])

def mean(ls):
    if ls:
        return sum(ls)/len(ls)
    return 0

def stddev(ls):
    if ls:
        return math.sqrt(mean([l**2 for l in ls])-mean(ls)**2)
    return 0

def main():
    parser = argparse.ArgumentParser(description='Analyze LLAMA benchmark log files')
    parser.add_argument('logfile', type=str,
                        help='Log file to analyze')
    parser.add_argument('-o', '--output', type=str,
                        default='output.csv',
                        help='Output csv file')

    args = parser.parse_args()

    log_reader = LogReader(args.logfile)
    experiments = log_reader.parse()

    def aggregate(ls, n):
        agg = []
        for i in range(0, len(ls), n):
            agg.append(sum(ls[i: i+n]))
        return agg

    with open(args.output, 'w') as output:
        for e in experiments:
            nm_time = [k for k in e.runtimes
                       if not k.with_madvise]
            wm_time = [k for k in e.runtimes
                       if k.with_madvise]

            agg_nm = aggregate(nm_time[0], 10000)
            agg_wm = aggregate(wm_time[0], 10000)

            speedup = [-(a-b)/a for a, b in zip (agg_nm, agg_wm)]
            output.write('alpha = %s\n' % e.params['PARAM_ALPHA'])

            for s in speedup:
                output.write('%s\n' % s)

if __name__=='__main__':
    main()
