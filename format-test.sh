#/bin/bash

# Check if there is something wrong with formatting
FORMAT=`diff -u <(cat src/*) <(clang-format src/*)`
if [[ ! -z $FORMAT ]]; then
    echo "${FORMAT}"
    exit 1
fi
FORMAT=`diff -u <(cat lib/*) <(clang-format lib/*)`
if [[ ! -z $FORMAT ]]; then
    echo "${FORMAT}"
    exit 1
fi
FORMAT=`diff -u <(cat include/*) <(clang-format include/*)`
if [[ ! -z $FORMAT ]]; then
    echo "${FORMAT}"
    exit 1
fi
