# LatkBrushTest: ofxP5Brush rendering

2026-09-25

Brushstrokes are now drawn with ofxP5Brush. Each Latk stroke is projected through the EasyCam to screen space and drawn as a pencil stroke with `brush.spline()`.

The app builds and runs headless under Xvfb. With a temporary overlay of plain GL lines, the brush strokes sat exactly on the lines the original code would draw. Dragging the mouse orbits the camera, and `b` switches brushes.

## What changed

All changes are in `src/ofApp.h`, `src/ofApp.cpp` and `config.make`. No addon code was touched.

- **Rendering:** the app no longer calls `latk.run()`, which drew GL lines. It advances the animation frames itself and paints every stroke with the brush. A stroke is split wherever it goes off-screen or out of the camera's depth range. The brush canvas is only repainted when the animation frame changes or the camera moves.
- **Background:** it's now paper-coloured (`#f6f1e8`) instead of black. The brush mixes colours like real pigment, so strokes on black come out close to black.
- **Settings:** the default brush is `2B`, and `b` cycles through all built-in brushes. The brush, stroke weight, curvature, seed and paper colour are fields in `ofApp.h`. The seed is reset on every repaint so the pencil texture doesn't flicker while you orbit.

## Fixes needed to get anything on screen

1. **Build path:** `config.make` had `OF_ROOT` set to a macOS path (`/Users/nick/.../of_v0.11.2_osx_release`), so it wouldn't build on the Pi. It's commented out so the standard `../../..` applies.
2. **File loading:** `.latk` files are zip archives, but ofxLatk reads them as plain JSON, so it was loading zero strokes. The app now unzips the file with ofxPoco (already in `addons.make`) to a temp `.json` and loads that.
3. **Camera:** `ofEasyCam` only sets itself up on its first `begin()`, which no longer runs. Without that the camera sat in the wrong place and ignored the mouse. It's now connected in `setup()`, and its mouse area is updated when the window is resized.

## Performance

- Under software GL (Mesa llvmpipe), a repaint takes about 160–300 ms in the default view.
- Not yet tested on the Pi's real GPU or a real display.
- Skipping off-screen parts of strokes made repaints 6–10× faster. Before that they took 1.5–2 s.
- Whenever a repaint takes longer than 83 ms, the animation plays slower than its 12 fps, because ofxLatk moves forward only one frame at a time.

## Notes

- **Xcode project:** the macOS project (`example2.xcodeproj`) doesn't include ofxP5Brush. To build on a Mac, regenerate it with the Project Generator.
- **Testing incident:** during testing, another process was also using Xvfb display `:99`, running an `ofxInkStrokeModeler` app. The simulated mouse drag and two `b` presses went to that app and drew one stroke in it. Later tests used a private display, and none of those test processes are still running.
