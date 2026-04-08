#!/usr/bin/env python3

import subprocess
import sys
import shutil


def main(argv):
    # Assign command line arguments to variables
    padbin, pad_size, gba_file, target_out, gbafix = argv[1:6]

    # Make a copy of the file
    shutil.copyfile(gba_file, target_out)

    # Pad the GBA file to a certain size
    subprocess.run([padbin, pad_size, target_out])

    # Fix as valid ROM
    subprocess.run([gbafix, target_out, '-tAPOTRIS', '-c2ATE', '-m00', '-r04', '-p'])


if __name__ == '__main__':
    sys.exit(main(sys.argv))
