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

experiments = []
while i < len(lines):
    line = lines[i]
    line = line.strip()
    if line != '==========START EXPERIMENT==========':
        i += 1
        continue
    a = Experiment(lines[i+1], lines[i+2])

    for j in range(5):
        while lines[i] != '==========LLAMA OUTPUT==========':
            i += 1
        output = {}
        i += 1
        while lines[i] != '==========END LLAMA OUTPUT==========':
            parts = lines[i].split(':')
            if len(parts) == 2:
                output[parts[0].strip()] = parts[1].strip()
            i += 1
        i += 1
        a.no_madvise_outputs.append(output)
            
    for j in range(5):
        while lines[i] != '==========LLAMA OUTPUT==========':
            i += 1
        output = {}
        i += 1
        while lines[i] != '==========END LLAMA OUTPUT==========':
            parts = lines[i].split(':')
            if len(parts) == 2:
                output[parts[0].strip()] = parts[1].strip()
            i += 1
        i += 1
        a.with_madvise_outputs.append(output)
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
