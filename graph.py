import pylab
from collections import OrderedDict
import argparse

class DataSet(object):
    def __init__(self, headers, ls):
        self.ls = ls
        self.headers = OrderedDict([(h,i) for i, h in enumerate(headers)])

    def filter(self, kwargs):
        a = DataSet(self.headers.keys(), [])
        for item in self.ls:
            add = True
            for key in kwargs:
                if item[self.headers[key]] != kwargs[key]:
                    add = False
            if add:
                a.ls.append(item)
        return a

    def restrict(self, args):
        a = DataSet([h for h in self.headers.keys() if h in args],
                    [])
        for item in self.ls:
            add = []
            for key in args:
                add.append(item[self.headers[key]])
            a.ls.append(add)
        return a

    def dimension(self, dim):
        return [h[self.headers[dim]] for h in self.ls]


def read_data(file_name):
    header = []
    lines = []
    with open(file_name) as f:
        lines = f.read().split('\n')
        header = [h.strip() for h in lines[0].split(',')]
        lines = [[float(s) for s in line.split(',')]
                 for line in lines[1:-1]]

    return DataSet(header, lines)


DEFAULT_PARAMS = {
    'alpha': 0.1,
    'Cache size': 1000,
    'Number of queries': 40000,
    'Epoch threshold': 4,
}

Y_LABELS = {
    'speedup': '% speedup',
    'time-madvise': 'Time (s)',
    'time-nomadvise': 'Time (s)',
    'memory-madvise': 'Buffer cache usage (MB)',
    'memory-nomadvise': 'Buffer cache usage (MB)',
    'overhead': 'Buffer cache usage with madvise/Buffer cache usage without madvise',
}

LEGENDS = {
    'memory-madvise': 'with madvise',
    'memory-nomadvise': 'without madvise',
    'time-madvise': 'with madvise',
    'time-nomadvise': 'without madvise',
}

MARKERS = ['bo-', 'r^-', 'g+-']

def graph_data(data, x, ys, restriction, title, output):
    filtered = data.filter({k: restriction[k] for k in restriction
                            if k != x})

    counter = 0

    for y in ys:
        if y == 'time':
            ydata = [100*(1-b/a) for a, b in zip(
                filtered.dimension('Time (no madvise; s)'),
                filtered.dimension('Time (madvise; s)')
            )]
        elif y == 'memory':
            ydata = [a/b for a, b in zip(
                filtered.dimension('Buffer Cache Usage (no madvise; MB)'),
                filtered.dimension('Buffer Cache Usage (madvise; MB)')
            )]
        elif y == 'time-nomadvise':
            ydata = filtered.dimension('Time (no madvise; s)')
        elif y == 'time-madvise':
            ydata = filtered.dimension('Time (madvise; s)')
        elif y == 'memory-nomadvise':
            ydata = filtered.dimension('Buffer Cache Usage (no madvise; MB)')
        elif y == 'memory-madvise':
            ydata = filtered.dimension('Buffer Cache Usage (madvise; MB)')
        
        if len(ys) > 1:
            pylab.plot(filtered.dimension(x), ydata, MARKERS[counter], label=LEGENDS[y])
        counter += 1

    # pylab.plot(*plots)

    pylab.xlabel(x)
    pylab.ylabel(Y_LABELS[ys[0]])
    pylab.title(title)
    pylab.grid(True)
    if len(ys) > 1:
        pylab.legend(loc=4)
    if output:
        pylab.savefig(output)
    pylab.show()


def main():
    parser = argparse.ArgumentParser(description='Plots CSV files generated by analyze.py')
    parser.add_argument('csvfile', type=str,
                        help='CSV file to plot')
    parser.add_argument('x', type=str,
                        help='x axis')
    parser.add_argument('y', type=str, nargs='+',
                        help='a list of y-axis to plot')

    # Filtering arguments
    parser.add_argument('--alpha', type=float,
                        default=DEFAULT_PARAMS['alpha'],
                        help='Alpha value to restrict to')
    parser.add_argument('--cache', type=float,
                        default=DEFAULT_PARAMS['Cache size'],
                        help='Cache size to restrict to')
    parser.add_argument('--num_queries', type=float,
                        default=DEFAULT_PARAMS['Number of queries'],
                        help='Number of queries to restrict to')
    parser.add_argument('--epoch', type=float,
                        default=DEFAULT_PARAMS['Epoch threshold'],
                        help='epoch threshold to restrict to')

    parser.add_argument('--title', type=str,
                        default='Untitled graph',
                        help='Title of graph')

    parser.add_argument('--output', type=str,
                        default='',
                        help='Output file of the graph')

    args = parser.parse_args()
    restriction = {'alpha': args.alpha,
                   'Cache size': args.cache,
                   'Number of queries': args.num_queries,
                   'Epoch threshold': args.epoch
                  }
    graph_data(read_data(args.csvfile), args.x, args.y, restriction, args.title, args.output)

if __name__ == '__main__':
    main()
