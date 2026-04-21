
from PIL import Image
img = Image.open('output.png')
px = list(img.getdata())
white = sum(1 for p in px if p[0] > 200)
print(f'total={len(px)}, white={white}')
