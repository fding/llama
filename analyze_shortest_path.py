import argparse
import math
import re
import numpy as np
import matplotlib.pyplot as plt

class Experiment(object):
    def __init__(self, commit, params):
        self.commit = commit
        self.outputs = []

class Results(dict):
    def __init__(self, *args, **kwargs):
        super(Results, self).__init__(*args, **kwargs)

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
        n = len(self.lines)
        j = self.wait_for_line('(NO_MADVISE)|(WITH_MADVISE)')
        if j >= max_i:
            return None
        else:
            self.current_line = j

        if self.lines[self.current_line].strip() == 'WITH_MADVISE':
            output.with_madvise = True
        else:
            output.with_madvise = False

        self.current_line = self.wait_for_line('TRIAL \d+')
        if self.current_line >= n: return None

        self.current_line = self.wait_for_line('==========BEFORE VM STAT==========')
        if self.current_line >= n: return None

        j = self.wait_for_line('==========LLAMA OUTPUT==========')
        if j >= n: return None

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

        self.current_line = j+1

        return output

    def parse(self):
        experiments = []
        n = len(self.lines)

        while self.current_line < n:
            self.current_line = self.wait_for_line('==========START EXPERIMENT==========')
            if self.current_line >= n:
                break

            a = Experiment(self.lines[self.current_line+1], '')
            self.current_line += 2 
            next_expt = self.wait_for_line('==========START EXPERIMENT==========')

            while True:
                if self.current_line >= next_expt:
                    break
                output = self.parse_trial(next_expt)
                if output:
                    a.outputs.append(output)
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

    with open(args.output, 'w') as output:
        output.write('Time (no madvise; s),Time (madvise; s),\n')
        for e in experiments:
            nm_time = np.array([floatify(k['Time']) for k in e.outputs
                                 if not k.with_madvise]);
            wm_time = np.array([floatify(k['Time']) for k in e.outputs
                                 if k.with_madvise]);

            for i in range(len(nm_time)):
                output.write('%s,%s\n' % (
                    nm_time[i],
                    wm_time[i],
                ))

            nm_time_filtered = np.array([time for time in nm_time if time > 1])
            wm_time_filtered = np.array([time for time in wm_time if time > 1])
            print "number of trials:", len(nm_time_filtered), len(wm_time_filtered)

            percentages = 100 * np.divide(nm_time_filtered - wm_time_filtered,
                                          nm_time_filtered)

            print percentages
            print "percentages (mean):", np.mean(percentages)
            print "% (stdev):", np.std(percentages)

            plt.plot([0, 500], [0, 500], 'k-', ls='--', lw=2)
            plt.scatter(nm_time, wm_time)
            plt.axis([0, 500, 0, 500])
            plt.xlabel('No madvise time (s)')
            plt.ylabel('Madvise time (s)')
            plt.show()

if __name__=='__main__':
    main()
