import re
f = open('output.log', 'r')

class Experiment(object):
    def __init__(self, commit, params):
        self.commit = commit
        self.params = dict([
            tuple(kv.split('=')) for kv in params.split(',')
        ])
        self.no_madvise_outputs = []
        self.with_madvise_outputs = []

lines = f.read().split('\n')
i = 0

def wait_for_line(s, i):
    p = re.compile(s)
    while not p.match(lines[i].strip()):
        i = i + 1
    return i

experiments = []
with_madvise = False

while i < len(lines):
    i = wait_for_line('==========START EXPERIMENT==========', i)
    a = Experiment(lines[i+1], lines[i+2])
    i = i + 3

    for j in range(5):
        i = wait_for_line('(NO_MADVISE)|(WITH_MADVISE)', i)
        if lines[i] == 'WITH_ADVISE':
            with_madvise = True
        else:
            with_madvise = False

        i = wait_for_line('TRIAL \d+', i)
        i = wait_for_line('==========LLAMA OUTPUT==========', i)
        j = wait_for_line('==========END LLAMA OUTPUT==========', i)
        output = {}
        for line in lines[i+1: j]:
            parts = line.split(':')
            if len(parts) == 2:
                output[parts[0].strip()] = parts[1].strip()
        i = j + 1

        if with_madvise:
            a.with_madvise_outputs.append(output)
        else:
            a.no_madvise_outputs.append(output)

    experiments.append(a)

def floatify(s):
    return float(s.split()[0])

newf = open('output.csv', 'w')

for e in experiments:
    nm_time = [floatify(k['Time']) for k in e.no_madvise_outputs]
    wm_time = [floatify(k['Time']) for k in e.with_madvise_outputs]
    newf.write('%s,%s,%s,%s,%s,%s\n' % (
        e.params['PARAM_ALPHA'], e.params['PARAM_CACHE_SIZE'], e.params['PARAM_NUM_VERTICES'], e.params['PARAM_EPOCH_THRESHOLD'],
        sum(nm_time)/len(nm_time),
        sum(wm_time)/len(wm_time))
    )
