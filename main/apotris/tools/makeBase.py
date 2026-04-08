#!/usr/bin/env python3

import subprocess
import sys


def main(argv):
    # Assign command line arguments to variables
    makerom, input_file, output_file, gbafix = argv[1:5]

    # Make the base GBA rom
    subprocess.run([makerom, input_file, output_file])

    # Fix as valid ROM
    subprocess.run([gbafix, output_file, '-tAPOTRIS', '-c2ATE', '-m00', '-r04'])


if __name__ == '__main__':
    sys.exit(main(sys.argv))
