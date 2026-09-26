# LatkP5Brush Architecture

`LatkP5Brush` is an openFrameworks application that combines 3D spatial animation data from [ofxLatk](https://github.com/n1ckfg/ofxLatk) with the 2D painterly, textured brush strokes of [ofxP5Brush](https://github.com/n1ckfg/ofxP5Brush). 

Rather than utilizing native 3D OpenGL line rendering, the application manually projects 3D stroke data into 2D space to be drawn onto a customized brush canvas.

## Core Components

### 1. File Loading and Extraction
`.latk` files are internally ZIP archives containing a JSON payload. Because `ofxLatk` natively expects a raw JSON string or uncompressed file, the application uses `Poco::Zip` (`resolveLatkJson`) to extract the internal `.json` document into a temporary directory before loading it into the `Latk` parser.

### 2. Animation Control
Instead of using `latk.run()`, which automatically handles both time progression and OpenGL line drawing, the application takes manual control over playback in `ofApp::update()`. It steps the animation frames forward based on the elapsed time (`checkInterval()`), allowing it to decouple the animation state from the 3D rendering pipeline.

### 3. Camera System (`ofEasyCam`)
Because native 3D drawing (`cam.begin()` / `cam.end()`) is completely bypassed, `ofEasyCam` does not receive its default initialization. To maintain orbital camera controls, the app manually hooks the camera into the input events via `cam.setEvents(ofEvents())` in `setup()`, and resizes its control area in `windowResized()`.

### 4. 3D-to-2D Projection & Rendering
The rendering system (`drawBrushStrokes()`) manages translating the 3D stroke points into the 2D painterly canvas:
- **Projection:** The application iterates through every stroke point in the current animation frame and uses the camera's projection matrix (`cam.worldToScreen()`) to map 3D world coordinates to 2D screen coordinates.
- **Clipping Optimization:** Points that project off-screen or out of the camera's depth range are discarded. Strokes are broken at these boundaries, vastly improving rendering performance by preventing `ofxP5Brush` from processing massive off-screen coordinates.
- **Spline Generation:** Contiguous 2D points are sent to `brush.spline()` to generate smooth curves. To maintain straight edges where intended, splines are only generated for strokes with 3 or more points.

### 5. Render Caching and Styling
- **Caching:** Repainting the brush strokes is an expensive operation. The application implements a `needsRedraw` flag, re-rendering the canvas only when the animation frame advances or the camera matrix changes.
- **Texture Stability:** The brush seed (`brushSeed`) is reset before every repaint, ensuring that the procedural textures of the brush strokes do not flicker or jump while the user orbits the camera.
- **Pigment Mixing:** `ofxP5Brush` relies on subtractive color mixing. The canvas uses a bright paper color (`#f6f1e8`) instead of the standard black background so that colored strokes render accurately.

## Addons Dependencies
* **ofxLatk:** Parsing and managing the Lightning Artist Tool Kit 3D stroke/frame data.
* **ofxP5Brush:** Rendering the 2D textures and spline-based brush strokes.
* **ofxPoco:** Providing `Poco::Zip` to extract `.latk` compressed archives.
