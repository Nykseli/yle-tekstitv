#/bin/bash

set -e

# Terminal colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

function run_test {
    # printf "Running $1..."
    $1
    if [ $? -eq 0 ]; then
        printf "$1... ${GREEN} SUCCESS${NC}\n"
    else
        printf "$1... ${RED} FAILED${NC}\n"
    fi
}

TEST_EXECS=`find tests -name "check_*" -executable -type f`
for TEST in $TEST_EXECS
do
    run_test $TEST
done

