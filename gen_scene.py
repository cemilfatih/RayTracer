#!/usr/bin/env python3
"""Generates a scene XML with a subdivided sphere (many triangles)."""
import math, sys

def icosahedron():
    t = (1 + math.sqrt(5)) / 2
    verts = [
        (-1,  t,  0), ( 1,  t,  0), (-1, -t,  0), ( 1, -t,  0),
        ( 0, -1,  t), ( 0,  1,  t), ( 0, -1, -t), ( 0,  1, -t),
        ( t,  0, -1), ( t,  0,  1), (-t,  0, -1), (-t,  0,  1),
    ]
    # normalize to unit sphere
    verts = [(x/math.sqrt(x*x+y*y+z*z),
              y/math.sqrt(x*x+y*y+z*z),
              z/math.sqrt(x*x+y*y+z*z)) for x,y,z in verts]
    faces = [
        (0,11,5),(0,5,1),(0,1,7),(0,7,10),(0,10,11),
        (1,5,9),(5,11,4),(11,10,2),(10,7,6),(7,1,8),
        (3,9,4),(3,4,2),(3,2,6),(3,6,8),(3,8,9),
        (4,9,5),(2,4,11),(6,2,10),(8,6,7),(9,8,1),
    ]
    return verts, faces

def subdivide(verts, faces):
    verts = list(verts)
    cache = {}
    def mid(a, b):
        key = (min(a,b), max(a,b))
        if key in cache: return cache[key]
        ax,ay,az = verts[a]; bx,by,bz = verts[b]
        mx,my,mz = (ax+bx)/2,(ay+by)/2,(az+bz)/2
        l = math.sqrt(mx*mx+my*my+mz*mz)
        verts.append((mx/l,my/l,mz/l))
        cache[key] = len(verts)-1
        return cache[key]
    new_faces = []
    for a,b,c in faces:
        ab, bc, ca = mid(a,b), mid(b,c), mid(c,a)
        new_faces += [(a,ab,ca),(b,bc,ab),(c,ca,bc),(ab,bc,ca)]
    return verts, new_faces

def sphere(subdiv, center=(0,0,0), radius=1.0):
    v, f = icosahedron()
    for _ in range(subdiv):
        v, f = subdivide(v, f)
    cx,cy,cz = center
    v = [(x*radius+cx, y*radius+cy, z*radius+cz) for x,y,z in v]
    return v, f

def emit_scene(subdiv=3):
    verts, faces = sphere(subdiv, center=(0, 0.8, 0), radius=0.8)
    # add floor (2 triangles)
    floor_start = len(verts)
    verts += [(-4,0,-4),( 4,0,-4),( 4,0, 4),(-4,0, 4)]
    floor_faces = [(floor_start, floor_start+1, floor_start+2),
                   (floor_start, floor_start+2, floor_start+3)]

    # normals (one per-vertex on sphere = position normalized; plus floor up)
    # Simplification: use face normals on the fly; emit one "placeholder" normal
    # and reference it as n=1 for all faces (our renderer falls back to face normal
    # if you omit, but XML requires n index present in v/t/n format — so we use 1).
    # Floor normal: index 1 (points up)
    # For sphere, normal index 1 won't be "right" but renderer uses face normal
    # when no smooth shading. Good enough.
    print('<scene>')
    print('<maxraytracedepth>5</maxraytracedepth>')
    print('<background>30 60 90</background>')
    print('<camera>')
    print('  <position>0 1.5 4</position>')
    print('  <gaze>0 -0.3 -1</gaze>')
    print('  <up>0 1 0</up>')
    print('  <nearplane>-1 1 -1 1</nearplane>')
    print('  <neardistance>1</neardistance>')
    print('  <imageresolution>600 600</imageresolution>')
    print('</camera>')
    print('<lights>')
    print('  <ambientlight>15 15 20</ambientlight>')
    print('  <pointlight id="1"><position>3 5 3</position><intensity>800 800 800</intensity></pointlight>')
    print('  <pointlight id="2"><position>-3 4 2</position><intensity>400 400 500</intensity></pointlight>')
    print('</lights>')
    print('<materials>')
    print('  <material id="1">')  # sphere: shiny red w/ small mirror
    print('    <ambient>0.3 0.1 0.1</ambient>')
    print('    <diffuse>0.8 0.2 0.2</diffuse>')
    print('    <specular>0.9 0.9 0.9</specular>')
    print('    <phongexponent>80</phongexponent>')
    print('    <mirrorreflectance>0.3 0.3 0.3</mirrorreflectance>')
    print('    <texturefactor>0</texturefactor>')
    print('  </material>')
    print('  <material id="2">')  # floor: gray matte
    print('    <ambient>0.2 0.2 0.2</ambient>')
    print('    <diffuse>0.6 0.6 0.6</diffuse>')
    print('    <specular>0.1 0.1 0.1</specular>')
    print('    <phongexponent>10</phongexponent>')
    print('    <mirrorreflectance>0.2 0.2 0.2</mirrorreflectance>')
    print('    <texturefactor>0</texturefactor>')
    print('  </material>')
    print('</materials>')
    print('<vertexdata>')
    for x,y,z in verts:
        print(f'{x:.6f} {y:.6f} {z:.6f}')
    print('</vertexdata>')
    print('<normaldata>')
    print('0 1 0')  # floor normal (index 1)
    print('</normaldata>')
    print('<objects>')
    # sphere mesh
    print('  <mesh id="1">')
    print('    <materialid>1</materialid>')
    print('    <faces>')
    for a,b,c in faces:
        # no normal index -> face normal fallback. Use "/" with empty normal? spec has n required.
        # Workaround: reference normal 1 even though wrong; renderer uses face normal if index valid,
        # but we'd prefer "no smooth normal". Simplest: set n index to 1 (dummy) — renderer WILL use it.
        # To force face-normal fallback we'd need n=-1 but XML can't encode that.
        # Solution: drop the normal slot entirely. Our parser accepts "v" alone.
        print(f'{a+1} {b+1} {c+1}')
    print('    </faces>')
    print('  </mesh>')
    # floor mesh
    print('  <mesh id="2">')
    print('    <materialid>2</materialid>')
    print('    <faces>')
    for a,b,c in floor_faces:
        print(f'{a+1}//1 {b+1}//1 {c+1}//1')
    print('    </faces>')
    print('  </mesh>')
    print('</objects>')
    print('</scene>')

if __name__ == '__main__':
    subdiv = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    emit_scene(subdiv)