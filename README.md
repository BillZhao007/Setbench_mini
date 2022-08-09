# SetBench Mini

Mini example used for temporary experiments on state-of-art concurrent indexes.

Refer to Trevor Brown's SetBench: https://bitbucket.org/trbot86/setbench/src/master/

## To support PAPI measurements in usage:

Installing PAPI
```
git clone https://bitbucket.org/icl/papi.git
cd papi/src
./configure
sudo sh -c "echo 2 > /proc/sys/kernel/perf_event_paranoid"
./configure
make -j
make test
sudo make install
sudo ldconfig
```

After installing PAPI, include Paths of PAPI in the following system variables.
```
export PAPI_HOME={where you install PAPI}
export PATH=$PATH:$PAPI_HOME/bin
export C_INCLUDE_PATH=$C_INCLUDE_PATH:$PAPI_HOME/include
export LIBRARY_PATH=$LIBRARY_PATH:$PAPI_HOME/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PAPI_HOME/lib
```
