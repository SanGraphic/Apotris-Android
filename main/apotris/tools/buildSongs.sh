#!/usr/bin/env bash

# Set -e to exit the script if any command fails
set -e

# Set -u to treat unset variables as an error
set -u

# Set -o pipefail to make pipelines fail if any command fails
set -o pipefail

# Function to check for python3 and provide installation instructions
check_python3() {
    if ! command -v python3 &> /dev/null; then
        echo "Python 3 is not installed."
        if [[ $(uname) == "Darwin" ]]; then
            echo "Please install it from https://www.python.org/downloads/ or use https://brew.sh/ for advanced users."
        else
            echo "Please install it from your package manager (e.g., 'sudo apt install python3' on Debian-based systems)."
        fi
        exit 1
    fi
}

# Call the function to check for python3
check_python3

# Get machine hardware name (aarch64, x86_64)
machine_hw_name=$(uname -m)

# Check the machine hardware name and set APE variable accordingly
if [[ $(uname) == "Darwin" ]]; then
    APE="sh"
elif [ "$machine_hw_name" = "aarch64" ]; then
    APE="tools/ape.aarch64"
    chmod +x $APE
elif [ "$machine_hw_name" = "arm64" ]; then
    APE="tools/ape.aarch64"
    chmod +x $APE
elif [ "$machine_hw_name" = "x86_64" ]; then
    APE="tools/ape.x86_64"
    chmod +x $APE
elif [ "$machine_hw_name" = "amd64" ]; then
    APE="tools/ape.x86_64"
    chmod +x $APE
else
    echo "Unsupported architecture."
    exit 1
fi

pushd audio || exit 1

extensions=(".mod" ".it" ".xm" ".s3m")

# Iterate over files and count their channels
for ext in "${extensions[@]}"; do
    # Check if any files with the current extension exist
    if ls *"$ext" &>/dev/null; then
        for file in *"$ext"; do
            if [[ $(uname) == "Darwin" ]]; then
                COUNT_APE=$APE
            else
                COUNT_APE=../$APE
            fi
            if ! $COUNT_APE ../tools/countchan.exe "$file"; then
                echo "Error counting channels for $file"
                #exit 1
            fi
        done
    fi
done

popd || exit 1

if ! python3 ./tools/buildSoundbank.py append; then
    echo "Error building soundbank"
    exit 1
fi

if ! $APE tools/gbfs.exe ./tools/audio.gbfs ./tools/soundbank.bin ./tools/effect_locations.bin; then
    echo "Error creating audio.gbfs"
    exit 1
fi

if ! $APE tools/padbin.exe 256 ./tools/Apotris-base.gba; then
    echo "Error padding Apotris-base.gba"
    exit 1
fi

if ! $APE tools/cat.exe tools/Apotris-base.gba ./tools/audio.gbfs > Apotris.gba; then
    echo "Error creating Apotris.gba"
    exit 1
fi

file="Apotris.gba"

if [[ $(uname) == "Darwin" ]]; then
    size=$(stat -f "%z" "$file")
else
    size=$(stat --format=%s "$file")
fi

power=1
while (( $power < $size )); do
  power=$((power * 2))
done

if (( $power > 33554432 )); then
  echo "File size too large ($size, max is 32MB). Try adding fewer songs."
  exit 1
fi

echo $power

if ! $APE tools/padbin.exe $power $file; then
    echo "Error padding Apotris.gba"
    exit 1
fi
