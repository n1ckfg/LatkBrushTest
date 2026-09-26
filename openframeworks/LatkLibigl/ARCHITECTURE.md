# Architecture of LatkLibigl

LatkLibigl is an openFrameworks application that loads and renders 3D drawings (Latk format) using high-quality volumetric tubes. By integrating with `libigl`, it achieves smooth interpolation and consistent twisting of brush strokes. The architecture is optimized for performance, pre-calculating and uploading geometry to the GPU so that playback has minimal CPU overhead.

## Core Components

### 1. Application Layer (`ofApp`, `main`)
The `ofApp` coordinates the data loading, updates, and rendering. 
- It extracts a `.latk` zip archive to read its JSON contents.
- It initializes the `LatkMeshCache` with the loaded stroke data.
- It handles user input for toggling rendering modes (lines vs. tubes), cycling through shaders, and reloading shaders on the fly.
- In the `draw()` loop, it either renders the original openFrameworks lines or delegates to the cached `StrokeMesh` and `StrokeShader` for volumetric rendering.

### 2. Stroke Mesh Generation (`StrokeMesh`)
`StrokeMesh` is responsible for building a closed 3D tube with rounded end caps around a polyline.
- **Smoothing (libigl):** It fits a smooth cubic Bezier spline (`igl::fit_cubic_bezier`) to the original stroke points to eliminate hand jitter. 
- **Adaptive Sampling:** The spline is sampled based on curvature and length constraints, creating denser geometry around sharp bends and sparser geometry on straight paths.
- **Rotation-minimizing Frames:** It computes a parallel-transport frame (`igl::rotation_matrix_from_directions`) along the spine, ensuring the tube doesn't twist unnaturally.
- **Tapering and Caps:** It manages stroke tapering based on configurable stroke settings and adds half-ellipsoid rounded end caps.
- **UV Mapping:** Texture coordinates (`u`, `v`) are generated such that `u` tracks length in tube circumferences (keeping texture aspect ratio isotropic) and `v` wraps evenly around the tube.

### 3. Mesh Caching and Batching (`LatkMeshCache`)
To allow real-time playback, meshes are generated and kept on the GPU.
- **Multithreading:** Frame building distributes individual strokes across multiple cores via `igl::parallel_for`.
- **Batching:** Each frame of a Latk layer is consolidated into a single Vertex Buffer Object (VBO). A frame's strokes are appended to the same index and vertex arrays, effectively allowing the entire frame to be drawn with a single OpenGL draw call.
- **Lifecycle:** It can build all frames upfront at load time or dynamically cache them on the first draw.

### 4. Custom Shaders (`StrokeShader`)
`StrokeShader` handles the visual appearance of the strokes, pairing custom fragment shaders with built-in vertex shaders.
- **Compatibility:** It dynamically wraps fragment shaders with boilerplate compatible with both GLSL 1.20 and 1.50 (programmable pipeline), abstracting away openFrameworks version differences.
- **Noise Texture Generator:** It provides a method `makeNoiseTexture()` which generates a seamlessly wrapping 2D value-noise texture across multiple frequency bands, for use in fragment shaders.
- **Hot-reloading:** Supports reloading `.frag` files at runtime for rapid visual iteration.

### 5. Fragment Shaders (`bin/data/shaders/`)
A collection of custom materials for the tubes:
- **Built-in Lit Shader:** Basic lighting model with diffuse, specular, and rim lighting.
- **fractalNoise.frag:** Implements a 3D value noise hash (fBm) evaluated over a wrapping cylindrical space. Uses `fwidth` to fade high-frequency octaves as they get smaller than a pixel, preventing aliasing and shimmering on distant strokes.
- **fractalNoiseTexture.frag:** An optimized variant of fractal noise that samples from the CPU-generated multi-band noise texture instead of hashing mathematically.

## Data Flow Pipeline
1. **Load:** `.latk` file extracted and parsed into raw polylines.
2. **Process:** `LatkMeshCache` iterates over frames and invokes `StrokeMesh`.
3. **Smooth & Sweep:** `StrokeMesh` fits Bezier curves via `libigl`, samples them, computes parallel transport frames, and constructs tubular vertices and indices.
4. **Upload:** `LatkMeshCache` packs all strokes in a frame into a single interleaved VBO.
5. **Render:** `ofApp` binds `StrokeShader`, activates the VBO from `LatkMeshCache`, and dispatches a single draw call per layer frame.

## Dependencies
- **openFrameworks:** Application framework, windowing, math, and OpenGL wrappers.
- **libigl:** Used for spline fitting (`fit_cubic_bezier`), parallelization (`parallel_for`), and rotation matrices.
- **ofxLatk:** Addon for reading and writing the Latk JSON format.
- **Poco:** Used within `ofApp` to unzip compressed `.latk` files on the fly.
