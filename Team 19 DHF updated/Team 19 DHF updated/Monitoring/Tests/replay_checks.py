"""Replay packaged monitoring checks without overwriting historical evidence."""
from pathlib import Path
import csv,ctypes,io,math,platform,shutil,subprocess,sys,tempfile
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
RESULTS=ROOT/'Results'
FIELDS=('dom_freq','p_low','p_tremor','p_high','ratio','prominence','total_pwr','tremor_amp')
class Features(ctypes.Structure):
    _fields_=[(name,ctypes.c_float) for name in FIELDS]

def read_csv(path):
    with path.open(newline='') as f:return list(csv.DictReader(f))

def close(a,b):return math.isclose(float(a),float(b),rel_tol=1e-7,abs_tol=1e-6)

def main():
    compiler=shutil.which('clang') or shutil.which('cc')
    if not compiler:raise RuntimeError('Install or select a C compiler (clang or cc).')
    source=ROOT/'Code/features.c'
    flags=['-O2','-Wall','-Wextra']
    print('Compiler:',subprocess.check_output([compiler,'--version'],text=True).splitlines()[0])
    print('Source:',source.name)
    with tempfile.TemporaryDirectory(prefix='monitoring-checks-') as folder:
        folder=Path(folder)
        executable=folder/'clean-check'
        subprocess.run([compiler,*flags,'-I',str(source.parent),str(ROOT/'Tests/known_frequency_check.c'),str(source),'-lm','-o',str(executable)],check=True)
        actual=list(csv.DictReader(io.StringIO(subprocess.check_output([str(executable)],text=True))))
        saved=read_csv(RESULTS/'clean_signal_results_2026-10-23.csv')
        assert len(actual)==len(saved)==17
        for index,(new,old) in enumerate(zip(actual,saved),1):
            for key,value in old.items():
                if key=='case':assert new[key]==value,(index,key)
                else:assert close(new[key],value),(index,key,new[key],value)
        print('Clean check: all 17 saved rows reproduced (15 signals and 2 controls).')
        library=folder/('features.dylib' if sys.platform=='darwin' else 'features.so')
        linkflag='-dynamiclib' if sys.platform=='darwin' else '-shared'
        subprocess.run([compiler,*flags,'-fPIC',linkflag,str(source),'-lm','-o',str(library)],check=True)
        lib=ctypes.CDLL(str(library))
        lib.vm_features.argtypes=[ctypes.POINTER(ctypes.c_float),ctypes.c_int,ctypes.POINTER(Features)]
        lib.vm_features.restype=None
        lib.vm_presence_gate.argtypes=[ctypes.POINTER(Features),ctypes.c_float]
        lib.vm_presence_gate.restype=ctypes.c_int
        saved=read_csv(RESULTS/'synthetic_signal_results_2026-11-09.csv')
        with np.load(RESULTS/'synthetic_inputs_2026-11-09.npz',allow_pickle=False) as bundle:
            samples=bundle['samples'];identifiers=bundle['case_ids']
        assert samples.shape==(194,624) and len(saved)==194
        counts={};misses=[]
        for row,data,case_id in zip(saved,samples,identifiers):
            assert str(case_id)==row['case_id']
            data=np.ascontiguousarray(data,dtype=np.float32)
            result=Features()
            lib.vm_features(data.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),len(data),ctypes.byref(result))
            rms=math.sqrt(float(np.mean(data.astype(np.float64)**2)))
            gate=lib.vm_presence_gate(ctypes.byref(result),rms)
            assert gate==int(row['gate']),(case_id,'gate',gate,row['gate'])
            for key in FIELDS:
                assert close(getattr(result,key),row[key]),(case_id,key,getattr(result,key),row[key])
            item=counts.setdefault(row['kind'],[0,0]);item[0]+=1;item[1]+=gate
            if row['kind']=='mixed_sines' and not gate:misses.append(str(case_id))
        print('Saved-input replay: all 194 rows reproduced, including all eight features and gate decisions.')
        for kind,(total,positive) in counts.items():print(f'{kind}: {positive}/{total} detected')
        print('Mixed-motion misses:',', '.join(misses))
        print('This verifies reproduction of the saved observations, not clinical performance or acceptance criteria.')
        print('Historical inputs and results were not modified.')
    print('Runtime:',platform.python_version(),'NumPy',np.__version__)

if __name__=='__main__':main()
