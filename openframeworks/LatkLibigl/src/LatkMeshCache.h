#pragma once

#include "ofMain.h"
#include "ofxLatk.h"
#include "StrokeMesh.h"

// Volumetric meshes for the strokes of a Latk drawing, made with
// makeStrokeMesh() and kept on the GPU. Each frame of each layer is merged
// into one vertex buffer, so drawing a layer is a single draw call.
//
// Frames are built the first time they're drawn, or all at once by build().
// The cache is indexed by layer and frame number: call invalidate() after
// changing the strokes or settings, and they'll be rebuilt when next drawn.
class LatkMeshCache {

	public:
		// Builds every frame of every layer, spreading the strokes over all cores.
		void build(const Latk & latk);
		// Draws the current frame of each layer. Vertex colours are the stroke
		// colours, and texture coordinates are as described in StrokeMesh.h.
		void draw(const Latk & latk);
		void invalidate();
		void invalidate(int layer, int frame);

		// Totals over the frames built so far.
		size_t getNumFramesBuilt() const;
		size_t getNumVertices() const;
		size_t getNumTriangles() const;
		size_t getNumBytes() const; // GPU buffer memory
		float getBuildSeconds() const;

		StrokeMeshSettings settings;

	private:
		struct FrameMesh {
			ofVbo vbo;
			size_t numVertices = 0;
			size_t numIndices = 0;
			bool built = false;
		};

		void buildFrames(const Latk & latk, const vector<glm::ivec2> & frames);
		void matchLayout(const Latk & latk);

		vector<vector<FrameMesh>> layers;
		float buildSeconds = 0;

};
