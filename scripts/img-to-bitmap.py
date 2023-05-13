#!/usr/bin/python

# small script to turn image files into single-bit bitmap structs
# usage: python img-to-bitmap.py <image file> [<C variable name>]

import sys
import numpy
from PIL import Image

if len(sys.argv) < 2 or len(sys.argv) > 3:
    print(f'Usage: {sys.argv[0]} <image file> [<C const name>]')
    exit(1)
    
img = Image.open(sys.argv[1])

if len(sys.argv) == 3:
    print(f'const bitmap::Bitmap {sys.argv[2]} = ', end='{\n')
else:
    print('(bitmap::Bitmap){')
    
print(f'    .width = {img.width},')
print(f'    .height = {img.height},')
print('    .data = (uint8_t[]){')

pixels = numpy.asarray(img)
i = 0
for row in pixels:
    for col in row:
        if i % 8 == 0:
            print('        0b', end='')
        print(int(numpy.any(col)), end='')
        if i % 8 == 7:
            print(',')
        i += 1

print('    }')
print('}', end='')

if len(sys.argv) == 3:
    print(';')
else:
    print()
