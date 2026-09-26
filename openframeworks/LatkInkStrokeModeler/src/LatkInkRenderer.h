#pragma once

#include "ofMain.h"
#include "ofxLatk.h"
#include "ofxInkStrokeModeler.h"

// How LatkInkRenderer shapes the strokes.
struct LatkInkSettings {
	// Latk stores the points of a stroke but not when they were drawn. The
	// strokes are replayed through the modeler as if one point came in at
	// this rate, in Hz. A faster rate means a faster pen, which the spring
	// lags further behind, so the stroke comes out smoother.
	float inputRate = 90;
	float width = 4; // pixels at full pressure; LatkStroke draws 4 px lines
	// Pressure, and with it the width, eases from endPressure at the tips to
	// 1 at taperLength in, in the stroke's own units (before globalScale).
	float endPressure = 0.35;
	float taperLength = 2.0;

	bool operator==(const LatkInkSettings & other) const;
	bool operator!=(const LatkInkSettings & other) const;
};

// Draws the current frame of each layer of a Latk drawing in ink. Each stroke
// is projected to the screen and traced there by ofxInkStrokeModeler, as if
// drawn with a pen, then filled as a ribbon with round caps.
//
// The modeler's parameters are tuned for pixels, so modeling happens in
// screen space. update() remodels whenever the camera, the viewport, the
// settings or a layer's current frame changes, and otherwise keeps the last
// result on the GPU.
class LatkInkRenderer {

	public:
		void setup();
		// Remodels if anything changed since the last call. Returns true if it did.
		bool update(const Latk & latk, const ofCamera & cam, const ofRectangle & viewport = ofGetCurrentViewport());
		// In screen coordinates, so call it outside cam.begin() / cam.end().
		void draw() const;
		// Forces the next update() to remodel, e.g. after editing the strokes.
		void invalidate();

		LatkInkSettings settings;

		// Totals for the last remodel, and how long its stages took.
		struct Stats {
			size_t strokes = 0; // pieces modeled; strokes are cut where they leave the view
			size_t inputs = 0;
			size_t results = 0;
			size_t vertices = 0;
			size_t triangles = 0;
			float projectMs = 0; // projection, clipping and pressure
			float modelMs = 0; // ofxInkStrokeModeler
			float meshMs = 0; // ribbons, merged into one mesh
			float uploadMs = 0;
			float totalMs = 0;
		};
		const Stats & getStats() const;

	private:
		void remodel(const Latk & latk, const glm::mat4 & modelViewProjection, const ofRectangle & viewport);

		ofxInkStrokeModeler modeler;
		ofVbo vbo;
		size_t numIndices = 0;
		Stats stats;

		// What the current mesh was modeled from.
		bool modeled = false;
		glm::mat4 modeledMatrix;
		ofRectangle modeledViewport;
		vector<int> modeledFrames;
		LatkInkSettings modeledSettings;

		// Reused between remodels, to save allocations.
		vector<glm::vec2> screenPoints;
		vector<bool> visible;
		vector<float> pressures;
		vector<glm::vec3> vertices;
		vector<ofFloatColor> colors;
		vector<ofIndexType> indices;

};
