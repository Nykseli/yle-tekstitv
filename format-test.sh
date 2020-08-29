#/bin/bash

# Check if there is something wrong.
# Diff exits with 1 if there is a diff to be found
diff -u <(cat src/*) <(clang-format src/*)
