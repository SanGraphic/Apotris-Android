#!/usr/bin/env python3

import glob
import sys

pattern = sys.argv[1]
sources = glob.glob(pattern)
for i in sources:
    print(i)