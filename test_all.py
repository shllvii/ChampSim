import subprocess
import os

path = '.'
trace_path = os.path.join(path, 'crc2_traces')
bin_path = os.path.join(path, 'bin')


nwarmup = '5'
ntest = '25'

algos = ['lru', 'chickeye', 'srrip', 'drrip', 'ship']
ncpus = ['1']

traces = os.listdir(trace_path)
print(traces)

for ncpu in ncpus:

    for algo in algos:
        print('Testing: ' + algo)
        bin_name = 'bimodal-no-no-no-no-'+algo+'-'+ncpu+'core'
        print('Find binary file ' + os.path.join(bin_path, bin_name))
        if not os.path.isfile(os.path.join(bin_path, bin_name)):
            print('|-- NOT found --')
            print('Build binary-file for '+algo)
            build_bin = subprocess.run(['./build_champsim.sh', 'bimodal', 'no', 'no', 'no', 'no', algo, ncpu], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        for trace in traces:
            if not os.path.isfile(os.path.join(path, 'results_'+ncpu+'core', trace+'-'+bin_name+'.txt')):
                print('|** test ' + trace + ' --')
                if ncpu=='1':
                    run_champsim = subprocess.run(['./run_champsim.sh', 'bimodal-no-no-no-no-'+algo+'-'+ncpu+'core', nwarmup, ntest, trace], stdout=subprocess.DEVNULL,  stderr=subprocess.DEVNULL,check=True)
                else:
                    run_champsim = subprocess.run(['./run_4core.sh','bimodal-no-no-no-no-'+algo+'-'+ncpu+'core', nwarmup, ntest, '4', trace, trace, trace, trace], stdout=subprocess.DEVNULL,  stderr=subprocess.DEVNULL,check=True)






