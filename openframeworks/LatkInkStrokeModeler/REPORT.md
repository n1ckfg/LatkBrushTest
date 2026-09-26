# LatkInkStrokeModeler: ink strokes

2026-09-26

Strokes are now drawn in ink with ofxInkStrokeModeler. Each Latk stroke is projected to the screen and replayed through the modeler as if drawn with a pen. The smoothed stroke is drawn as a tapered ribbon with round caps. The modeling runs on all four cores. While the camera moves, a frame is remodeled in 4–5 ms (median, whole drawing in view), down from 10–16 ms in the first version.

The app builds and runs on the Pi 4 and was tested on its GPU (V3D 4.2). Drawn together, the ribbons sit on Latk's original GL lines, which run down the middle of each one. Dragging orbits the camera, `s` changes how much the modeler smooths, and `l` switches to the original lines.

## What changed

No addon code was touched. `addons.make` now lists ofxInkStrokeModeler as well as ofxLatk and ofxPoco.

- **`src/LatkInkRenderer.*`:** `update(latk, cam)` remodels the current frame of each layer if the camera, window, settings or frame changed, and `draw()` draws the result in screen coordinates:
  1. Points are projected with the camera's matrix, computed once per remodel. Strokes are cut where they leave the camera's depth range or a margin around the window, and each visible run is modeled as a stroke of its own.
  2. Latk records no timing, so the points are fed to the modeler as if they arrived at a steady `inputRate`: 90 Hz by default, a typical VR headset rate.
  3. The file has no pressure either. Each point gets one that eases from 35% at the tips to full width over `taperLength` (2 units) of the stroke, like the tubes in LatkLibigl. It sets the ribbon width through a `toMesh()` width function, at 4 px at full pressure, the width of Latk's lines.
  4. The ribbons from each thread go into one vertex buffer, so a frame takes four draw calls. Strokes still overlap in their original order.
- **`src/ofApp.*`:** the app advances the animation itself instead of calling `latk.run()`, and draws ink or the original lines. `l` switches between them, and `s` cycles the input rate through 90, 180 and 360 Hz. A faster rate means a faster pen, which the modeled spring lags further behind, so corners round off and jitter smooths out. The status line shows the last remodel time and point counts. `o` still writes `test.latk`.

## Fixes needed

1. **Build path:** `config.make` had `OF_ROOT` set to a macOS path. It's commented out, as in LatkP5Brush, so the standard `../../..` applies.
2. **File loading:** as in the other integrations, `.latk` files are zip archives and ofxLatk only reads plain JSON. The document is unzipped with ofxPoco first.
3. **Camera:** the ink isn't drawn inside `cam.begin()`, where ofEasyCam connects itself to the mouse. It's connected in `setup()`, and its mouse area follows the window size, as in LatkP5Brush.

## Performance

**Setup.** As for LatkLibigl: a private headless labwc compositor with Xwayland, drawing offscreen into a buffer the size of the app's window. Timings run from the first GL call to `glFinish()`, over two loops of the animation (148 frames), with all modes drawing the same frames in turn. There are two views. *Default* is the app's starting camera, which is inside the jellyfish and sees the tentacles close up. *Whole* is from the side and far enough out to see the whole jellyfish. To force a remodel on every frame, the camera turned 0.5° per frame. Timings are median and 90th percentile, in ms. The harness lived outside the repo.

**Remodeling while the camera moves**, at 1024×768. The *Whole* view has 5,930 points per frame on average, in 112 pieces.

| Version | Default | Whole | Modeler results (whole) | Ribbon vertices (whole) |
|---|---|---|---|---|
| First version | 10.6 / 19.5 | 16.4 / 27.3 | 18,295 | 39,733 |
| Two modeler steps per input instead of three | 8.3 / 15.2 | 12.1 / 20.5 | 12,432 | 28,008 |
| Pressure looked up by time, not modeled | 7.1 / 12.8 | 10.2 / 17.5 | 12,432 | 28,008 |
| Modeling on 4 threads | 5.3 / 8.6 | 6.6 / 9.9 | 12,432 | 28,008 |
| **Ribbon points at least 1 px apart, one buffer per thread (final)** | **4.3 / 6.1** | **5.0 / 6.7** | **12,432** | **13,737** |

- **Extra steps:** the modeler takes `ceil(interval × 180 Hz)` steps between inputs, with the interval rounded to float. At exactly 90 Hz, 1/90 s rounds up and gives 3 steps instead of 2, which is 50% more work for nothing (60 and 180 Hz round up too). Asking for a hair under 180 Hz gives the intended count.
- **Pressure:** given pressure, the modeler's stylus stage projects every result back onto the input to interpolate it. On the CPU alone, that was 45% of the modeling time (7.7 against 4.5 ms per frame). The inputs are evenly spaced in time, so the width function now looks pressure up by each result's time. It looks 24 ms earlier: the spring's mass × drag, the time by which the modeled pen trails the input at steady speed.
- **Threads:** the pieces are split into four shares of similar point counts, each with its own modeler. Building took 2.0 ms instead of 5.9. Starting and joining the threads costs 0.12 ms, so a thread pool wasn't worth adding.
- **Ribbon points:** the modeler outputs at least 180 points a second. At zooms like the whole view, 64–92% of them were under a pixel apart. Skipping points closer than 1 px halved the ribbons. It changed 230 of about 37,000 lit pixels in a close-up. Drawing one buffer per thread saves merging them, and uploading now takes 0.46 ms instead of 1.3.

**Final breakdown** (whole view, 1024×768): projecting takes 0.50 ms. Modeling and ribbons take 1.77 ms on four threads (4.2 ms of modeler time and 1.3 ms of ribbon building in total), and uploading takes 0.46 ms. That's 2.7 ms of CPU; the rest is the GPU clearing and drawing.

**Drawing** (final version):

| Mode | 1024×768, default | 1024×768, whole | 1920×1080, default | 1920×1080, whole |
|---|---|---|---|---|
| Empty frame (clear only) | 1.7 / 1.8 | 1.7 / 1.9 | 4.4 / 4.7 | 4.4 / 5.1 |
| Original GL lines | 3.2 / 4.1 | 3.3 / 4.2 | 5.9 / 6.8 | 6.0 / 7.2 |
| **Ink, camera still** | **2.1 / 2.4** | **2.2 / 2.4** | **4.9 / 5.1** | **4.9 / 5.5** |
| **Ink, camera moving** | **4.3 / 6.1** | **5.0 / 6.7** | **7.5 / 9.8** | **8.0 / 11.2** |

- **Camera still:** the ribbons stay on the GPU and only need drawing, which is cheaper than the original lines. The strokes are still remodeled whenever the animation advances, 12 times a second.
- **Camera moving:** every frame fits in 60 fps, even at 1080p and on busy frames.
- **Input rate:** at 180 and 360 Hz each input takes one step instead of two, so remodeling is cheaper: 4.5 ms (median, whole view) at both.
- **Antialiasing:** the ribbons have hard edges, like the lines. 4× MSAA on the offscreen buffer made them smooth, but added about 5 ms to every mode, lines included: 9.6 ms for moving ink in the default view. It's left off.

## Notes

- **Screen space:** the modeler smooths each stroke as it appears on screen, so in principle the result depends on the view. The spring model at its core is linear, so zooming scales the smoothing along with the stroke. Only the wobble smoother and the end-of-stroke stopping distance work in absolute pixels.
- **Depth:** the ribbons are flat and drawn in stroke order without depth testing, as the original lines are.
- **Rounding in the modeler:** the extra step comes from the library, which matches upstream. It only matters for exact synthetic timestamps like these; real input devices jitter. It could be fixed in ofxInkStrokeModeler, but that would break its byte-for-byte match with upstream.
- **Camera:** unchanged from the original app, so it starts inside the jellyfish, as in the other integrations. Drag to orbit, right-drag to zoom out.
- **Test displays:** a private headless labwc session (`wayland-0` / `:0`), stopped afterwards.
