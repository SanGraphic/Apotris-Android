#!/usr/bin/env python3

import qrcode
import sys
from PIL import Image as image
import numpy as np

from os.path import exists

import struct

url = sys.argv[1]

qr = qrcode.QRCode(
    version = None,
    error_correction=qrcode.constants.ERROR_CORRECT_L,
    box_size = 1,
    border = 2,
)

qr.add_data(url)
qr.make(fit=True)

img = qr.make_image(fill_color="black", back_color="white")

output_file = sys.argv[2]

print(url)

pixels = img.load()

w,h = img.size

print(f"{h} {w}")

fout = open(output_file,'wb')

for y in range(h):
    for x in range(w):
        colors = []

        for i in range(3):
            index = i * 2 + 1;
            colors.append(int(pixels[x,y]))

        result = ""
        for c in colors:
            c = int(int(c)/8)
            bin = "{0:b}".format(c);
            while len(bin) < 5:
                bin = "0"+ bin
            result = bin+result

        num = int(result,2)
        fout.write(struct.pack('<h',num))

fout.close()
