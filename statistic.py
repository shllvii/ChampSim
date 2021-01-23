import os

path = '.'
trace_path = os.path.join(path,'crc2_traces')

cores = ['1', '4']
algolists = [['lru', 'srrip', 'drrip', 'ship', 'hawkeye','chickeye'], ['lru', 'srrip', 'drrip', 'ship','hawkeye','chickeye']]
traces = [s.split('.')[0] for s in os.listdir(trace_path)]
print(traces)

if not os.path.isdir(os.path.join(path,'statistics')):
    os.mkdir(os.path.join(path,'statistics'))

for core,algolist in zip(cores,algolists) :
    filename = os.path.join(path, 'statistics', 'data'+core+'.csv')
    result_path = os.path.join(path, 'results_'+core+'core')
    with open(filename, 'w') as f:
        f.write('Strategy,' + ','.join([s.split('_')[0] for s in traces]) + '\n')
        for algo in algolist:
            f.write(algo)
            for trace in traces:
                result = os.path.join(result_path, trace+'.trace.xz-bimodal-no-no-no-no-'+algo+'-'+core+'core.txt')
                access, hit = 0, 0
                total_access, total_hit = 0, 0
                with open(result, 'r') as rs:
                    for line in rs.readlines():
                        words = line.split()
                        #if len(words) > 6 and words[0] == 'LLC' and (words[1] == 'LOAD' or words[1] == 'RFO'):
                        if len(words) > 6 and words[0] == 'LLC' and words[1] == 'TOTAL' :
                            access += int(words[3])
                            hit += int(words[5])
                f.write(','+str(100*hit//access)+'%')
            f.write('\n')







