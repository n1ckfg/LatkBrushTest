# Architecture of LatkInkStrokeModeler

LatkInkStrokeModeler is an openFrameworks application that plays back a Latk animation drawn in ink. It projects each 3D stroke onto the screen, and traces it there with [ofxInkStrokeModeler](https://github.com/n1ckfg/ofxInkStrokeModeler), a port of Google's Ink Stroke Modeler, as if it were being drawn with a pen. The smoothed result is filled as a tapered ribbon with round caps.

## Directory Structure
- `src/`
  - `main.cpp` - Opens a 1024×768 window and runs `ofApp`.
  - `ofApp.h` & `ofApp.cpp` - Loads the drawing, steps the animation, handles input, and switches between ink and Latk's original GL lines.
  - `LatkInkRenderer.h` & `LatkInkRenderer.cpp` - Projects, models and draws the strokes.
- `addons.make` - `ofxLatk`, `ofxInkStrokeModeler` and `ofxPoco`.
- `bin/data/` - The `.latk` drawings.

## Core Components

### 1. Application Layer (`ofApp`)
- **Loading:** a `.latk` file is a zip archive holding one JSON document, and `ofxLatk` only reads plain JSON. `resolveLatkJson()` extracts the document with `Poco::Zip` to a temp file before handing it to `Latk`.
- **Animation:** instead of `latk.run()`, which both advances and draws, `update()` advances each layer's frame at Latk's 12 fps and `draw()` does the drawing.
- **Camera:** the ink is projected by hand and never drawn inside `cam.begin()`, which is where `ofEasyCam` would connect itself to the mouse. `setup()` connects it with `cam.setEvents()`, and `windowResized()` updates its mouse area.
- **Controls:** `l` switches between ink and the original lines, `s` cycles the input rate (90, 180, 360 Hz), `o` writes `test.latk`.

### 2. Ink Rendering (`LatkInkRenderer`)
The modeler works in 2D and its parameters are tuned for pixels, so strokes are modeled in screen space. `update()` compares the camera matrix, viewport, settings and each layer's current frame with those of the last remodel. If nothing changed, the ribbons already on the GPU are drawn again. Otherwise it remodels in three stages:

1. **Project** (on the calling thread): every point of the current frames goes through the camera's model-view-projection matrix, computed once, as `ofCamera::worldToScreen()` would do it. Strokes are cut where they leave the camera's depth range or a margin around the viewport, and each visible run becomes a *piece*. Each point also gets a pressure: it eases from 35% at the tips to full within `taperLength` of each end, measured along the whole stroke, so the pieces of a cut stroke keep their widths.
2. **Model and mesh** (one worker per core): the pieces are split into contiguous shares with similar point counts. Each worker has its own `ofxInkStrokeModeler`, and replays each piece through it with `kDown`, `kMove` and `kUp` inputs, timestamped as if one point arrived every `1 / inputRate` seconds. Latk doesn't record timing. Results closer than `minSpacing` (1 px) are skipped, and `ofxInkStrokeModeler::toMesh()` turns the rest into a ribbon, with a width function that sets the width from pressure.
3. **Upload**: each worker's ribbons go to its own VBO, and `draw()` draws the workers' VBOs in order. That keeps the drawing's stroke order, so overlaps are the same as with the original lines.

The modeler has no prediction (the strokes are finished), and two things differ from its defaults:

- **Output rate:** it takes `ceil(interval × min_output_rate)` steps between inputs, with the interval rounded to float. At input rates that divide 180 Hz, that rounds up to an extra step, so `min_output_rate` is set a hair under 180 Hz.
- **Pressure:** it isn't passed to the modeler, whose stylus stage would project every result back onto the input. Since inputs are evenly spaced in time, the width function looks the pressure up by each result's time instead. It looks `spring_mass_constant × drag_constant` seconds earlier, the time by which the modeled pen trails the input when moving steadily.

## Data Flow Pipeline
1. **Load:** `.latk` unzipped with Poco and parsed by `ofxLatk` into layers, frames and strokes of 3D points.
2. **Advance:** `ofApp::update()` steps each layer's current frame.
3. **Project:** `LatkInkRenderer` projects the current frames to the screen and cuts them into visible pieces with per-point pressure.
4. **Model:** worker threads replay the pieces through `ofxInkStrokeModeler` and build ribbons with `toMesh()`.
5. **Draw:** one VBO per worker, drawn in screen coordinates after the camera, with vertex colours from the strokes.

## Dependencies
- **openFrameworks:** windowing, camera, math and OpenGL wrappers.
- **ofxLatk:** reads and writes Latk data, and draws the original GL lines.
- **ofxInkStrokeModeler:** models the pen strokes and builds the ribbon meshes.
- **ofxPoco:** `Poco::Zip`, to extract `.latk` archives.
