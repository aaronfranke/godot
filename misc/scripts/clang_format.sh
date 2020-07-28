#!/usr/bin/env bash

# This script runs clang-format and fixes copyright headers on all relevant files in the repo.
# This is the primary script responsible for fixing style violations.

set -uo pipefail
IFS=$'\n\t'

CLANG_FORMAT_FILE_EXTS=(".c" ".h" ".cpp" ".hpp" ".cc" ".hh" ".cxx" ".m" ".mm" ".inc" ".java" ".glsl")

# Loops through all text files tracked by Git.
git grep -zIl '' |
while IFS= read -rd '' f; do
    # Exclude some files.
    if [[ "$f" == "thirdparty"* ]]; then
        continue
    elif [[ "$f" == "platform/android/java/lib/src/com/google"* ]]; then
        continue
    elif [[ "$f" == *"-so_wrap."* ]]; then
        continue
    fi

    for extension in ${CLANG_FORMAT_FILE_EXTS[@]}; do
        if [[ "$f" == *"$extension" ]]; then
            # Run clang-format.
            clang-format -i "$f"
            # Fix copyright headers, but not all files get them.
            if [[ "$f" == *"inc" ]]; then
                continue 2
            elif [[ "$f" == *"glsl" ]]; then
                continue 2
            elif [[ "$f" == *"theme_data.h" ]]; then
                continue 2
            elif [[ "$f" == "platform/android/java/lib/src/org/godotengine/godot/input/InputManager"* ]]; then
                continue 2
            fi
            python misc/scripts/copyright_headers.py "$f"
            continue 2
        fi
    done
done
