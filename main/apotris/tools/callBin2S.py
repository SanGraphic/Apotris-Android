#!/usr/bin/env python3

import subprocess
import sys
import os


def main():
    bin2s_path = sys.argv[1]
    input_bin = sys.argv[2]
    header_h = sys.argv[3]
    output_s = sys.argv[4]
    platform = sys.argv[5]
    xxd = sys.argv[6]

    if platform == 'apple':
        return_code = subprocess.run([bin2s_path, '--apple-llvm', '-a', '4', '-H', header_h, input_bin],
                                     stdout=open(output_s, 'w'))
        with open(header_h, 'r') as file:
            lines = file.readlines()
        with open(header_h, 'w') as file:
            for line in lines:
                line = line.replace('uint8_t _', 'uint8_t ')
                line = line.replace('size_t _', 'size_t ')
                file.write(line)
    elif platform == 'gba':
        args = [bin2s_path, '-a', '4', '-H', header_h, input_bin]
        if os.name == 'nt':
            args.insert(0, sys.executable)
        return_code = subprocess.run(args, stdout=open(output_s, 'w'))
    else:
        args = [bin2s_path, '-a', '4', '-H', header_h, input_bin]
        if os.name == 'nt':
            args.insert(0, sys.executable)
        return_code = subprocess.run(args, stdout=subprocess.DEVNULL)
        with open(output_s, 'w') as file:
            file.write('#include \"{}\"\n'.format(header_h))
        xxd_output = subprocess.check_output([xxd, '-i', input_bin]).decode('utf-8').split('\n')
        with open(output_s, 'a') as file:
            for line in xxd_output:
                line = line.replace('___', '')
                line = line.replace('data_', '')
                line = line.replace('unsigned char', 'const uint8_t')
                file.write(line + '\n')

    if return_code.returncode != 0:
        print("bin2s command failed.")
        sys.exit(1)

    with open(output_s, 'a') as file:
        file.write('\n')


if __name__ == "__main__":
    main()
