# Mini example used for temporary experiments in research.
Refer to Trevor Brown's SetBench: https://bitbucket.org/trbot86/setbench/src/master/

### To support PAPI measurements in usage:
After installing PAPI, include Paths of PAPI in the following system variables.
```
export PAPI_HOME={where you install PAPI}
export PATH=$PATH:$PAPI_HOME/bin
export C_INCLUDE_PATH=$C_INCLUDE_PATH:$PAPI_HOME/include
export LIBRARY_PATH=$LIBRARY_PATH:$PAPI_HOME/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PAPI_HOME/lib
```
