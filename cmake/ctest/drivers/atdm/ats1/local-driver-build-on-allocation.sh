#!/bin/bash
export CTEST_SITE=${ATDM_CONFIG_CDASH_HOSTNAME}
export CTEST_DO_TEST=FALSE
srun -N1 $WORKSPACE/Trilinos/cmake/ctest/drivers/atdm/ctest-s-driver.sh
