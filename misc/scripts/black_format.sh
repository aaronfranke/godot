#!/usr/bin/env bash

# This script runs black on all Python files in the repo.

set -uo pipefail

# Apply black.
echo -e "Formatting Python files..."
PY_FILES=$(find \( -path "./.git" \
                -o -path "./thirdparty" \
                \) -prune \
                -o \( -name "SConstruct" \
                -o -name "SCsub" \
                -o -name "*.py" \
                \) -print)
black -l 120 $PY_FILES
