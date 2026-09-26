# Architecture of `ofxLatk` Example

## Overview
This directory contains a basic openFrameworks example application demonstrating the integration of the `ofxLatk` addon. The application is designed to load, display, and export LATK (Lightning Artist Toolkit) animation files.

## Directory Structure
- `src/` - Contains the application source code.
  - `main.cpp` - The application entry point. Configures the OpenGL window and runs the main `ofApp` instance.
  - `ofApp.h` & `ofApp.cpp` - The main application class extending `ofBaseApp`. Contains the primary logic for loading and rendering LATK data.
- `addons.make` - Specifies the required openFrameworks addons for building this project (`ofxLatk` and `ofxPoco`).
- `Makefile` & `config.make` - Standard openFrameworks build configuration files.
- `bin/` - The output directory for the compiled executable and data assets (e.g., the `.latk` files).

## Core Components

### `ofApp` Class
The main application class handles the openFrameworks lifecycle and user input:

- **`setup()`**: Initializes the application by loading a LATK data file (`jellyfish.latk`) into a `Latk` object instance.
- **`draw()`**: Responsible for rendering the LATK data each frame. It uses an `ofEasyCam` to provide interactive 3D camera controls (pan, tilt, zoom) around the rendered strokes.
- **`keyPressed()`**: Handles user input. Pressing the 'o' key triggers an export, writing the current LATK data out to a new file named `test.latk`.

### Dependencies
- **openFrameworks**: Provides the core application windowing, rendering, and input event system.
- **ofxLatk**: The primary addon being demonstrated, used for parsing, writing, and drawing LATK data structures.
- **ofxPoco**: An openFrameworks wrapper for the POCO C++ Libraries, listed as a required addon in `addons.make`.
