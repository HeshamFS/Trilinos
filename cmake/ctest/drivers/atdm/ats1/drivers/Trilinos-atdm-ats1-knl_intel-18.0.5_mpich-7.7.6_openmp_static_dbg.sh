#!/bin/bash
if [ "${Trilinos_TRACK}" == "" ] ; then
  export Trilinos_TRACK=Experimental
fi
export SBATCH_TIME_LIMIT_MINUTES=1440 # 24 hr time limit
$WORKSPACE/Trilinos/cmake/ctest/drivers/atdm/ats1/local-driver.sh
