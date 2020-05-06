#!/bin/bash -l

if [ "${SBATCH_TIME_LIMIT_MINUTES}" == "" ] ; then
  SBATCH_TIME_LIMIT_MINUTES=180  # Default limit is 3 hours
fi

if [ "${Trilinos_CTEST_DO_ALL_AT_ONCE}" == "" ] ; then
  export Trilinos_CTEST_DO_ALL_AT_ONCE=TRUE
fi

# Load environment on the "this" node
source $WORKSPACE/Trilinos/cmake/std/atdm/load-env.sh $JOB_NAME

set -x

atdm_config_sbatch_extra_args=$ATDM_CONFIG_SBATCH_EXTRA_ARGS
unset ATDM_CONFIG_SBATCH_EXTRA_ARGS
# Run configure, build on a haswell compute node
atdm_run_script_on_compute_node \
  $WORKSPACE/Trilinos/cmake/ctest/drivers/atdm/ats1/local-driver-build-on-allocation.sh \
  $PWD/ctest-s-driver.out \
  ${SBATCH_TIME_LIMIT_MINUTES}

export ATDM_CONFIG_SBATCH_EXTRA_ARGS=$atdm_config_sbatch_extra_args
 # Run tests on either a KNL or HSW compute node
atdm_run_script_on_compute_node \
  $WORKSPACE/Trilinos/cmake/ctest/drivers/atdm/ats1/local-driver-test-on-allocation.sh \
  $PWD/ctest-s-driver.out \
  ${SBATCH_TIME_LIMIT_MINUTES} 
