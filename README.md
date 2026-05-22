# Ray Tracer — Build and Run Instructions

## Requirements

- macOS or Linux
- A C++17 compiler (tested with Apple Clang 17, but g++ ≥ 7 also works)
- `make`
- No other dependencies — TinyXML-2 and stb_image are bundled in `external/`

## Build

```bash
make
```

This produces an executable named `raytracer` in the project root.

To clean:

```bash
make clean
```

## Run

Standard invocation:

```bash
./raytracer <scene.xml> [output.png]
```

If `output.png` is omitted, the result is written to `output.png` in the current directory.

### Examples

```bash
./raytracer scenes/standing_girl.xml result.png
./raytracer some_test_scene.xml
```

The renderer prints the scene summary, BVH statistics, render-time, and the output filename to standard output / standard error.

## CLI flags

| Flag | Effect |
|------|--------|
| `--no-bvh`     | Disable the BVH acceleration structure (use brute-force triangle iteration). For benchmarking only — *much* slower on dense meshes. |
| `--no-threads` | Disable multithreading (single-threaded render). For benchmarking only. |

Examples:

```bash
./raytracer scene.xml out.png --no-bvh                  # BVH off, threads on
./raytracer scene.xml out.png --no-threads              # BVH on, threads off
./raytracer scene.xml out.png --no-bvh --no-threads     # naive baseline
```

By default both features are enabled.

## Scene file format

The XML scene file follows the format defined in the assignment specification. Key points:

- `<scene>` is the root element.
- One `<textureimage>` per scene (the renderer supports the assignment's "single texture per scene" rule).
- Texture file paths in `<textureimage>` are resolved **relative to the directory of the XML file**.
- Material `id` attributes can be any string; faces inside `<mesh>` reference materials by that string in `<materialid>`.
- Vertex / normal / UV indices in `<faces>` are 1-based, in the OBJ-style `v/t/n` notation. Missing components (e.g. `v//n` or `v/t`) are accepted.

## Output

Output is PNG, RGB, 8-bit per channel. Dimensions are taken from `<imageresolution>` in the scene file.

## Performance

On a MacBook with 8 logical cores, the included `scenes/standing_girl.xml` (18,192 triangles, 800×800) renders in:

- ~80 ms (BVH + threads, default)
- ~370 ms (BVH only)
- ~11.5 s (threads only)
- ~50 s (naive — both flags off)

For higher resolutions the time scales roughly linearly with pixel count.


