import sys, os, time, argparse
try:
    import cPickle
except ImportError:
    import pickle as cPickle
from mpi4py import MPI
from ReadWriteBlitzArray import writeASCIIBlitzArrayToDisk
from numpy import array

def prepare_SAI(params_simu, simuDirName):
    my_id = MPI.COMM_WORLD.Get_rank()
    tmpDirName = os.path.join(simuDirName, 'tmp' + str(my_id))
    file = open(os.path.join(tmpDirName, 'pickle', 'variables.txt'), 'rb')
    variables = cPickle.load(file)
    file.close()
    # writing the chunk numbers (per process)
    chunkNumbers = variables['processNumber_to_ChunksNumbers'][my_id]
    writeASCIIBlitzArrayToDisk(array(chunkNumbers).astype('i'), os.path.join(tmpDirName, 'Mg_LeftFrob', 'chunkNumbers.txt'))
    # writing the cubes numbers (chunk-dependent)
    for chunk in chunkNumbers:
        writeASCIIBlitzArrayToDisk(array(variables['chunkNumber_to_cubesNumbers'][chunk]).astype('i'), os.path.join(tmpDirName, 'Mg_LeftFrob', "chunk" + str(chunk) + 'cubesNumbers.txt'))
    # writing the chunk numbers (cube-dependent)
    writeASCIIBlitzArrayToDisk(array(variables['cubeNumber_to_chunkNumber']).astype('i'), os.path.join(tmpDirName, 'Mg_LeftFrob', 'cubeNumber_to_chunkNumber.txt')) 
    variables['Wall_time_Mg_computation'] = 0.0
    variables['CPU_time_Mg_computation'] = 0.0
    file = open(os.path.join(tmpDirName, 'pickle', 'variables.txt'), 'wb')
    cPickle.dump(variables, file)
    file.close()

if __name__=='__main__':
    parser = argparse.ArgumentParser(description='...')
    parser.add_argument('--inputdir')
    parser.add_argument('--simudir')
    cmdline = parser.parse_args()
    simuDirName = cmdline.simudir
    inputDirName = cmdline.inputdir
    simuParams = 'simulation_parameters'

    # the simulation itself
    my_id = MPI.COMM_WORLD.Get_rank()
    if (my_id==0):
        sys.path.append(os.path.abspath(inputDirName))
        exec('from ' + simuParams + ' import *')
    else:
        params_simu = ['blabla']
    params_simu = MPI.COMM_WORLD.bcast(params_simu)
    if (params_simu.MONOSTATIC_RCS==1) or (params_simu.MONOSTATIC_SAR==1) or (params_simu.BISTATIC==1):
        prepare_SAI(params_simu, simuDirName)
    else:
        print("you should select monostatic RCS or monostatic SAR or bistatic computation, or a combination of these computations. Check the simulation settings.")
        sys.exit(1)

