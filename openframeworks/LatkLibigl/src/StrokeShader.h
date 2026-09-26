#pragma once

#include "ofMain.h"

// Pairs a fragment shader with a vertex shader for stroke meshes, so a new
// look only needs a .frag file. The fragment shader gets:
//
//   varying vec2 vUv;       texture coordinates (see StrokeMesh.h)
//   varying vec3 vNormal;   normal in eye space, not normalised
//   varying vec3 vPosition; position in eye space
//   varying vec4 vColor;    stroke colour
//   uniform float time;     seconds since the app started
//
// Write it in GLSL 1.20 (varying, gl_FragColor, texture2D) with no #version
// line; it's adapted to GLSL 1.50 when openFrameworks runs the programmable
// renderer. Error line numbers match the file.
class StrokeShader {

	public:
		// Loads a fragment shader file, or the built-in lit shader if the path
		// is empty. If it doesn't compile, the previous shader stays in use.
		bool load(const string & fragmentPath = "");
		bool reload();

		void begin();
		void end();

		// The fragment shader's file name, or "lit" for the built-in shader.
		string getName() const;

	private:
		ofShader shader;
		string path;

};
