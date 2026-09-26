#include "LatkMeshCache.h"

#pragma push_macro("PI")
#pragma push_macro("TWO_PI")
#undef PI
#undef TWO_PI
#include <igl/parallel_for.h>
#pragma pop_macro("TWO_PI")
#pragma pop_macro("PI")

namespace {

// Interleaved, so one buffer feeds every attribute.
struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;
	ofFloatColor color;
};

}

//--------------------------------------------------------------
void LatkMeshCache::build(const Latk & latk) {
	matchLayout(latk);
	vector<glm::ivec2> frames;
	for (size_t i = 0; i < latk.layers.size(); i++) {
		for (size_t j = 0; j < latk.layers[i].frames.size(); j++) frames.emplace_back(i, j);
	}
	buildFrames(latk, frames);
}

//--------------------------------------------------------------
void LatkMeshCache::draw(const Latk & latk) {
	matchLayout(latk);

	// Build any missing frames together, so their strokes share the threads.
	vector<glm::ivec2> missing;
	for (size_t i = 0; i < latk.layers.size(); i++) {
		const int frame = latk.layers[i].currentFrame;
		if (frame >= 0 && frame < (int)layers[i].size() && !layers[i][frame].built) missing.emplace_back(i, frame);
	}
	if (!missing.empty()) buildFrames(latk, missing);

	for (size_t i = 0; i < latk.layers.size(); i++) {
		const int frame = latk.layers[i].currentFrame;
		if (frame < 0 || frame >= (int)layers[i].size()) continue;
		const FrameMesh & mesh = layers[i][frame];
		if (mesh.numIndices > 0) mesh.vbo.drawElements(GL_TRIANGLES, mesh.numIndices);
	}
}

//--------------------------------------------------------------
void LatkMeshCache::invalidate() {
	// Replacing the meshes frees their GPU buffers now rather than at rebuild.
	for (auto & frames : layers) {
		for (auto & mesh : frames) mesh = FrameMesh();
	}
}

//--------------------------------------------------------------
void LatkMeshCache::invalidate(int layer, int frame) {
	if (layer >= 0 && layer < (int)layers.size() && frame >= 0 && frame < (int)layers[layer].size()) {
		layers[layer][frame] = FrameMesh();
	}
}

//--------------------------------------------------------------
void LatkMeshCache::buildFrames(const Latk & latk, const vector<glm::ivec2> & frames) {
	const float startTime = ofGetElapsedTimef();

	// Flatten to one job per stroke, so long frames don't hold up a thread.
	struct Job {
		const LatkStroke * stroke;
		ofMesh mesh;
	};
	vector<Job> jobs;
	vector<size_t> firstJob;
	for (auto & f : frames) {
		firstJob.push_back(jobs.size());
		for (auto & stroke : latk.layers[f.x].frames[f.y].strokes) jobs.push_back({&stroke, ofMesh()});
	}
	firstJob.push_back(jobs.size());

	igl::parallel_for(jobs.size(), [&](size_t i) {
		const vector<glm::vec3> points(jobs[i].stroke->points.begin(), jobs[i].stroke->points.end());
		jobs[i].mesh = makeStrokeMesh(points, settings);
	}, 2);

	// Merge each frame's strokes and upload them. GL calls stay on this thread.
	vector<Vertex> vertices;
	vector<ofIndexType> indices;
	for (size_t f = 0; f < frames.size(); f++) {
		vertices.clear();
		indices.clear();
		for (size_t i = firstJob[f]; i < firstJob[f + 1]; i++) {
			const LatkStroke & stroke = *jobs[i].stroke;
			const ofMesh & mesh = jobs[i].mesh;
			const ofIndexType offset = vertices.size();
			// Like LatkStroke::draw(), scale each stroke by its globalScale.
			// Scaling the whole tube keeps the normals and texture coordinates valid.
			for (size_t v = 0; v < mesh.getNumVertices(); v++) {
				vertices.push_back({mesh.getVertex(v) * stroke.globalScale, mesh.getNormal(v), mesh.getTexCoord(v), stroke.strokeColor});
			}
			for (auto index : mesh.getIndices()) indices.push_back(index + offset);
		}

		FrameMesh & frameMesh = layers[frames[f].x][frames[f].y];
		frameMesh = FrameMesh();
		frameMesh.built = true;
		frameMesh.numVertices = vertices.size();
		frameMesh.numIndices = indices.size();
		if (indices.empty()) continue;

		ofBufferObject vertexBuffer, indexBuffer;
		vertexBuffer.allocate(vertices, GL_STATIC_DRAW);
		indexBuffer.allocate(indices, GL_STATIC_DRAW);
		const int stride = sizeof(Vertex);
		frameMesh.vbo.setVertexBuffer(vertexBuffer, 3, stride, offsetof(Vertex, position));
		frameMesh.vbo.setNormalBuffer(vertexBuffer, stride, offsetof(Vertex, normal));
		frameMesh.vbo.setTexCoordBuffer(vertexBuffer, stride, offsetof(Vertex, texCoord));
		frameMesh.vbo.setColorBuffer(vertexBuffer, stride, offsetof(Vertex, color));
		frameMesh.vbo.setIndexBuffer(indexBuffer);
	}

	buildSeconds += ofGetElapsedTimef() - startTime;
}

//--------------------------------------------------------------
void LatkMeshCache::matchLayout(const Latk & latk) {
	layers.resize(latk.layers.size());
	for (size_t i = 0; i < layers.size(); i++) layers[i].resize(latk.layers[i].frames.size());
}

//--------------------------------------------------------------
size_t LatkMeshCache::getNumFramesBuilt() const {
	size_t total = 0;
	for (auto & frames : layers) {
		for (auto & mesh : frames) total += mesh.built;
	}
	return total;
}

//--------------------------------------------------------------
size_t LatkMeshCache::getNumVertices() const {
	size_t total = 0;
	for (auto & frames : layers) {
		for (auto & mesh : frames) total += mesh.numVertices;
	}
	return total;
}

//--------------------------------------------------------------
size_t LatkMeshCache::getNumTriangles() const {
	size_t total = 0;
	for (auto & frames : layers) {
		for (auto & mesh : frames) total += mesh.numIndices / 3;
	}
	return total;
}

//--------------------------------------------------------------
size_t LatkMeshCache::getNumBytes() const {
	size_t total = 0;
	for (auto & frames : layers) {
		for (auto & mesh : frames) total += mesh.numVertices * sizeof(Vertex) + mesh.numIndices * sizeof(ofIndexType);
	}
	return total;
}

//--------------------------------------------------------------
float LatkMeshCache::getBuildSeconds() const {
	return buildSeconds;
}
