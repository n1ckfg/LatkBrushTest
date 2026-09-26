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

		// Binds texture to the sampler uniform `name` in this and any later
		// shader, and sets `vec2 <name>Size` to its size in pixels.
		void setTexture(const string & name, const ofTexture & texture);

		void begin();
		void end();

		// The fragment shader's file name, or "lit" for the built-in shader.
		string getName() const;

		// Random values for value noise that wraps seamlessly around a tube.
		// The texture is a stack of bands: band b has 2^b rows, then a copy of
		// its first row so that filtering wraps too. It starts at row
		// 2^b - 1 + b. Sample it with GL_LINEAR, as shaders/fractalNoise.frag
		// does, to get one octave of noise per texture read.
		static ofTexture makeNoiseTexture(int width = 256, int bands = 7);

	private:
		ofShader shader;
		string path;
		vector<pair<string, ofTexture>> textures;

};
