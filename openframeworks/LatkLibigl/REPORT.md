# LatkLibigl: volumetric stroke meshes

2026-09-26

Brushstrokes are now drawn as tubes built with libigl. The meshes are cached on the GPU and shaded with a fragment shader that can be swapped out. The default shader is a fractal noise (fBm) pattern laid out in each tube's uv space. It reads its noise from a small texture, which made it 25–33% faster than computing the noise in the shader.

The app builds and runs on the Pi 4. It was tested on the Pi's own GPU (V3D 4.2), and under software GL (llvmpipe) for comparison. From the same camera, the tubes follow Latk's original GL lines stroke for stroke.

## What changed

No addon code was touched. `addons.make` now lists ofxEigen, ofxLibigl and ofxPoco as well as ofxLatk.

- **`src/StrokeMesh.*`:** `makeStrokeMesh(polyline, settings)` turns a polyline into a closed tube (`ofMesh`) with normals and uvs:
  1. `igl::fit_cubic_bezier` fits a smooth spline to the points, within `fitTolerance` × radius. This removes hand jitter: about 50 points per stroke become about 5 curves.
  2. The spline is evaluated finely with `igl::bezier`. The derivative is a Bezier too, so tangents are exact. A ring is kept only where the curve has turned `maxSegmentAngle` (15°) or run `maxSegmentLength` (4 radii) since the last one. Straight runs get few rings; bends get many.
  3. The rings are oriented by parallel transport (`igl::rotation_matrix_from_directions`), so tubes don't twist. Arc length comes from `igl::cumsum`.
  4. The radius eases from 35% at the tips to full size, and the ends are rounded caps. A one-point stroke becomes a sphere. The normals account for the taper.
  5. uv.x runs along the stroke, measured in tube circumferences, so the uv space isn't stretched. uv.y goes from 0 to 1 around the tube. The seam vertices are doubled so uv.y never wraps inside a triangle.
  6. Triangles face outward (checked by signed volume on edge cases: single point, duplicate points, closed loop, hairpin, NaN input). This lets back faces be culled.
- **`src/LatkMeshCache.*`:** keeps one vertex buffer per frame of each layer, so each layer draws with one call. `build()` makes every frame at startup, spreading strokes over the four cores with `igl::parallel_for`. `draw()` builds any frame it hasn't seen yet, and `invalidate()` marks frames stale after edits. Each stroke is scaled by its `globalScale`, as `LatkStroke::draw()` does.
- **`src/StrokeShader.*`:** `load("shaders/x.frag")` pairs any fragment shader with a built-in vertex shader. That vertex shader provides `vUv`, `vNormal`, `vPosition`, `vColor` and a `time` uniform. Shaders are written in GLSL 1.20 and adapted automatically if openFrameworks runs its programmable renderer. Error line numbers match the file. If an edit doesn't compile, the previous shader keeps running. `setTexture(name, texture)` gives shaders a texture, along with its size as `<name>Size`.
- **`bin/data/shaders/fractalNoiseTexture.frag`** (the default): five octaves of value noise. It drifts along each stroke over time. Octaves fade out as they shrink below a few pixels, which stops distant strokes shimmering and skips work. Each octave is one read from `StrokeShader::makeNoiseTexture()`:
  - The texture holds 256 columns of random values in stacked bands, and band *b* repeats every 2^b rows.
  - Each band ends with a copy of its first row. Octave *k* reads the band that repeats every 2^k cells around the tube, so hardware filtering wraps around the tube without a seam.
  - Bilinear filtering at smoothstepped positions gives smooth noise from a single read, instead of eight hashes.
- **`bin/data/shaders/fractalNoise.frag`:** the same pattern computed in the shader, with 3D value noise on the uv space wrapped onto a cylinder. It needs no texture but is slower.
- **Controls:** `s` cycles through the texture fBm shader, the computed one, and a plain lit one. `r` reloads the shader from disk. `l` switches to the original GL lines for comparison. `o` still writes `test.latk`.

## Fixes needed

1. **File loading:** as in LatkP5Brush, `.latk` files are zip archives and ofxLatk only reads plain JSON, so the app was loading zero strokes. The document is unzipped with ofxPoco first.
2. **Macro clash:** openFrameworks includes `<termios.h>`, whose `B0` macro breaks `igl/fit_cubic_bezier.cpp`. It's suspended around the includes, like `PI` and `TWO_PI`. The ofxLibigl README only mentions those two.

## Performance

**Setup.** The Pi has no display attached, so tests on the GPU used a private headless labwc compositor with Xwayland. Each mode was drawn offscreen into a 1024×768 buffer (the app's window size), and timed from the first GL call to `glFinish()`. All modes drew the same animation frames in rotating order, over two loops of the animation (148 frames). Timings are median and 90th percentile, in ms.

**Meshes** (1,963 strokes and 98,149 points across 75 frames):

| Ring spacing | Rings per stroke | Triangles | GPU memory | Build time |
|---|---|---|---|---|
| First version: rings per curve estimated from control points | 62.6 | 1.90 M | 72.4 MB | 1.30 s |
| Rings kept by measured bend (tolerance 0.25) | 37.8 | 1.13 M | 43.5 MB | 1.15 s |
| **Final (tolerance 0.5)** | **33.0** | **0.97 M** | **37.8 MB** | **1.11 s** |

The first version's estimate of how much each curve bends was far too high. Measuring the bend on the evaluated curve halved the mesh size. Built on one core, the meshes take 2.9 s, and about 85% of that is `igl::fit_cubic_bezier`.

**Drawing** (V3D GPU):

| Mode | 1024×768 | 1920×1080 |
|---|---|---|
| Empty frame (clear only) | 1.7 / 2.0 | 4.4 / 6.5 |
| Original GL lines | 3.3 / 4.2 | 6.0 / 10.2 |
| Tubes, flat colour | 2.4 / 3.0 | 5.2 / 7.5 |
| Tubes, lit | 2.7 / 3.7 | 6.1 / 8.6 |
| Tubes, fBm computed in the shader | 5.3 / 10.8 | 13.9 / 21.8 |
| Tubes, fBm computed, no back-face culling | 7.1 / 14.3 | 19.5 / 29.9 |
| **Tubes, fBm from noise texture (default)** | **4.0 / 6.6** | **9.4 / 13.3** |

- **Lit tubes are cheaper than the original lines on the GPU.** The lines rebuild a shape for every stroke on the CPU each frame. The tubes need no CPU work per frame, only two draw calls.
- **Noise texture:** reading the noise from a texture made the fBm shader 25–33% faster on a median frame, and 39% faster on busy ones. The noise itself, measured as the cost above the lit shader at 1080p, fell from 7.8 to 3.3 ms. Close-ups from all around a tube showed no seam and no banding, and the pattern looks the same as the computed version.
- **Culling** saves 25% with the fBm shader.
- **Skipping octaves** once they fade out cut the fBm shader from 8.2 to 5.2 ms. Fading them when cells shrink below 8 pixels rather than 4 saved another 20% at 1080p. These two were measured with an earlier, finer noise scale. The final scale is coarser so that the pattern shows at the default view distance, which costs somewhat more.
- **Tried and not kept:** A depth pre-pass was no faster (5.7 ms against 5.3 ms): the GPU already skips hidden pixels. 2D noise that tiles around the tube uses half the hashes but was no faster. 4 octaves instead of 5 made no difference, because the fifth rarely survives the fade.
- **Software GL (llvmpipe)** gives the opposite result: 6.0 ms for lines, but 16.8 ms for lit tubes and 23–24 ms for fBm, because the pixel work runs on the CPU. There, the noise texture is only 5% faster. The LatkP5Brush numbers were measured this way.

**With the noise texture, even busy frames fit in 60 fps at 1080p. At the app's window size, every mode fits. The app itself runs at about 60 fps with vsync.**

**Still open:**

- **Fullscreen headroom:** at 1080p an empty frame alone takes 4.4 ms, and the texture fBm leaves about 3 ms spare on busy frames. Anything heavier at fullscreen will need fewer octaves, a coarser scale, or rendering at a lower resolution.
- **Memory:** colour takes 16 of the 48 bytes per vertex. Drawing each frame's strokes grouped by colour (at most four colours per frame here) would save about 9 MB, at the cost of a few more draw calls.
- **Startup:** building everything takes 1.1 s. Removing the `build()` call makes frames build as they first appear (about 15 ms each on the first loop), or the fitting could move to a background thread.

## Notes

- **Xcode project:** `example2.xcodeproj` doesn't include the new files or addons. Regenerate it with the Project Generator to build on a Mac.
- **Camera:** it's unchanged from the original app, and the jellyfish's bell is often above the default view. Drag to orbit.
- **Test displays:** testing used a private headless labwc session (`wayland-0` / `:0`) and Xvfb `:77`. Both were stopped afterwards. The benchmark harness lived outside the repo.
