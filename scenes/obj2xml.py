#!/usr/bin/env python3
"""Convert a .obj file to the PA1 scene XML format.
Usage: python3 obj2xml.py input.obj > scene.xml
"""
import sys

if len(sys.argv) < 2:
    print("usage: obj2xml.py input.obj", file=sys.stderr)
    sys.exit(1)

vertices = []    # list of "x y z" strings
normals  = []
uvs      = []
faces    = []    # list of list of (v, t, n) tuples, 1-based

with open(sys.argv[1]) as f:
    for line in f:
        parts = line.strip().split()
        if not parts:
            continue
        if parts[0] == 'v':
            vertices.append(f"{parts[1]} {parts[2]} {parts[3]}")
        elif parts[0] == 'vn':
            normals.append(f"{parts[1]} {parts[2]} {parts[3]}")
        elif parts[0] == 'vt':
            uvs.append(f"{parts[1]} {parts[2]}")
        elif parts[0] == 'f':
            toks = parts[1:]
            # each token is "v/t/n", "v//n", or "v"
            verts = []
            for tok in toks:
                bits = tok.split('/')
                v = int(bits[0])
                t = int(bits[1]) if len(bits) > 1 and bits[1] else 0
                n = int(bits[2]) if len(bits) > 2 and bits[2] else 0
                verts.append((v, t, n))
            # triangulate n-gons as a fan
            for i in range(1, len(verts) - 1):
                faces.append([verts[0], verts[i], verts[i+1]])

def fmt(v, t, n):
    if t == 0 and n == 0: return f"{v}"
    if t == 0: return f"{v}//{n}"
    if n == 0: return f"{v}/{t}"
    return f"{v}/{t}/{n}"

# --- emit XML ---
print('<scene>')
print('<maxraytracedepth>5</maxraytracedepth>')
print('<background>30 60 90</background>')
print('<camera>')
print('  <position>0 1.0 3</position>')
print('  <gaze>0 -0.1 -1</gaze>')
print('  <up>0 1 0</up>')
print('  <nearplane>-1 1 -1 1</nearplane>')
print('  <neardistance>1</neardistance>')
print('  <imageresolution>600 600</imageresolution>')
print('</camera>')
print('<lights>')
print('  <ambientlight>20 20 25</ambientlight>')
print('  <pointlight id="1"><position>2 4 3</position><intensity>800 800 800</intensity></pointlight>')
print('  <pointlight id="2"><position>-3 3 2</position><intensity>400 400 500</intensity></pointlight>')
print('</lights>')
print('<materials>')
print('  <material id="1">')   # model material
print('    <ambient>0.2 0.15 0.15</ambient>')
print('    <diffuse>0.7 0.6 0.6</diffuse>')
print('    <specular>0.3 0.3 0.3</specular>')
print('    <phongexponent>30</phongexponent>')
print('    <mirrorreflectance>0 0 0</mirrorreflectance>')
print('    <texturefactor>0</texturefactor>')
print('  </material>')
print('  <material id="2">')   # floor material
print('    <ambient>0.2 0.2 0.2</ambient>')
print('    <diffuse>0.5 0.5 0.5</diffuse>')
print('    <specular>0.2 0.2 0.2</specular>')
print('    <phongexponent>10</phongexponent>')
print('    <mirrorreflectance>0.3 0.3 0.3</mirrorreflectance>')
print('    <texturefactor>0</texturefactor>')
print('  </material>')
print('</materials>')

# Add a floor (4 extra vertices beyond the model)
floor_base = len(vertices) + 1    # 1-based index of first floor vertex
vertices.append("-5 0 -5")
vertices.append(" 5 0 -5")
vertices.append(" 5 0  5")
vertices.append("-5 0  5")
# Floor normal index (1-based)
floor_normal_idx = len(normals) + 1
normals.append("0 1 0")

print('<vertexdata>')
for v in vertices: print(v)
print('</vertexdata>')

if uvs:
    print('<texturedata>')
    for u in uvs: print(u)
    print('</texturedata>')

print('<normaldata>')
for n in normals: print(n)
print('</normaldata>')

print('<objects>')
# Model mesh
print('  <mesh id="1">')
print('    <materialid>1</materialid>')
print('    <faces>')
for face in faces:
    print(f'{fmt(*face[0])} {fmt(*face[1])} {fmt(*face[2])}')
print('    </faces>')
print('  </mesh>')
# Floor mesh
print('  <mesh id="2">')
print('    <materialid>2</materialid>')
print('    <faces>')
print(f'{floor_base}//{floor_normal_idx} {floor_base+1}//{floor_normal_idx} {floor_base+2}//{floor_normal_idx}')
print(f'{floor_base}//{floor_normal_idx} {floor_base+2}//{floor_normal_idx} {floor_base+3}//{floor_normal_idx}')
print('    </faces>')
print('  </mesh>')
print('</objects>')
print('</scene>')