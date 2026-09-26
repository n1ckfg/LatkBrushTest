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
	// The modeler outputs at least 180 points a second, often less than a
	// pixel apart on screen. The ribbons skip points closer than this, in pixels.
	float minSpacing = 1;

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
// result on the GPU. Strokes are modeled on all cores.
class LatkInkRenderer {

	public:
		// numThreads 0 uses one per core.
		void setup(size_t numThreads = 0);
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
			float modelMs = 0; // ofxInkStrokeModeler, summed over threads
			float meshMs = 0; // ribbons, summed over threads
			float buildMs = 0; // modeling and ribbons, as the threads ran them
			float uploadMs = 0;
			float totalMs = 0;
		};
		const Stats & getStats() const;

	private:
		// A run of a stroke's points that stays in view. It's modeled as a
		// stroke of its own.
		struct Piece {
			size_t first = 0; // into points and pressures
			size_t count = 0;
			ofFloatColor color;
		};

		// Models a share of the pieces on a thread of its own. Its ribbons are
		// drawn with one call, in turn, so strokes overlap as in the drawing.
		struct Worker {
			ofxInkStrokeModeler modeler;
			vector<ofxInkStrokeModeler::Result> kept; // results spaced for the ribbon
			vector<glm::vec2> vertices;
			vector<ofFloatColor> colors;
			vector<ofIndexType> indices;
			ofVbo vbo;
			size_t numIndices = 0;
			uint64_t modelMicros = 0;
			uint64_t meshMicros = 0;
			size_t numResults = 0;
		};

		void project(const Latk & latk, const glm::mat4 & modelViewProjection, const ofRectangle & viewport);
		void build(Worker & worker, size_t firstPiece, size_t endPiece);
		void upload();

		vector<Worker> workers;
		Stats stats;

		// What the current ribbons were modeled from.
		bool modeled = false;
		glm::mat4 modeledMatrix;
		ofRectangle modeledViewport;
		vector<int> modeledFrames;
		LatkInkSettings modeledSettings;

		// Reused between remodels, to save allocations.
		vector<Piece> pieces;
		vector<glm::vec2> points; // screen positions
		vector<float> pressures;
		vector<glm::vec2> strokePoints;
		vector<bool> strokeVisible;
		vector<float> strokePressures;

};
