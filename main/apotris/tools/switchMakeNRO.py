#!/usr/bin/env python3

import subprocess
import os
import shutil
import tempfile
import sys

def main():
    elf2nro_path = sys.argv[1]
    nacptool_path = sys.argv[2]
    icon_path = sys.argv[3]
    output_binary = sys.argv[4]
    version = sys.argv[5]

    with tempfile.TemporaryDirectory() as temp_dir:
        args = [nacptool_path, "--create", "Apotris", "akouzoukos", "v" + version, os.path.join(temp_dir, 'control.nacp')]
        return_code = subprocess.run(args)

        if return_code.returncode != 0:
            print("nacptool command failed.")
            sys.exit(1)

        args = [elf2nro_path, output_binary, output_binary + '.nro', f'--nacp={os.path.join(temp_dir, "control.nacp")}', f'--icon={icon_path}']
        #return_code = subprocess.run(args, stdout=subprocess.DEVNULL)
        return_code = subprocess.run(args)

        if return_code.returncode != 0:
            print("elf2nro command failed.")
            sys.exit(1)


if __name__ == "__main__":
    main()
