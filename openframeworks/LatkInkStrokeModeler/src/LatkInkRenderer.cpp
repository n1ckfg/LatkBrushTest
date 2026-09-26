#include "LatkInkRenderer.h"

namespace {

float millisSince(uint64_t startMicros) {
	return (ofGetElapsedTimeMicros() - startMicros) / 1000.f;
}

// 0 at x = 0, easing up to 1 at x = length.
float ease(float x, float length) {
	if (length <= 0) return 1;
	const float t = ofClamp(x / length, 0, 1);
	return t * t * (3 - 2 * t);
}

}

//--------------------------------------------------------------
bool LatkInkSettings::operator==(const LatkInkSettings & other) const {
	return inputRate == other.inputRate && width == other.width && endPressure == other.endPressure && taperLength == other.taperLength;
}

//--------------------------------------------------------------
bool LatkInkSettings::operator!=(const LatkInkSettings & other) const {
	return !(*this == other);
}

//--------------------------------------------------------------
void LatkInkRenderer::setup() {
	// The strokes are finished, so there's nothing to predict.
	modeler.setup(ofxInkStrokeModeler::Predictor::Disabled);
}

//--------------------------------------------------------------
bool LatkInkRenderer::update(const Latk & latk, const ofCamera & cam, const ofRectangle & viewport) {
	// As ofCamera::worldToScreen() projects, but computed once for all points.
	glm::mat4 matrix = cam.getModelViewProjectionMatrix(viewport);
	if (cam.isVFlipped()) matrix = glm::scale(glm::mat4(1), glm::vec3(1, -1, 1)) * matrix;

	vector<int> frames;
	for (auto & layer : latk.layers) frames.push_back(layer.currentFrame);

	if (modeled && matrix == modeledMatrix && viewport == modeledViewport && frames == modeledFrames && settings == modeledSettings) {
		return false;
	}

	remodel(latk, matrix, viewport);
	modeled = true;
	modeledMatrix = matrix;
	modeledViewport = viewport;
	modeledFrames = frames;
	modeledSettings = settings;
	return true;
}

//--------------------------------------------------------------
void LatkInkRenderer::draw() const {
	if (numIndices > 0) vbo.drawElements(GL_TRIANGLES, numIndices);
}

//--------------------------------------------------------------
void LatkInkRenderer::invalidate() {
	modeled = false;
}

//--------------------------------------------------------------
const LatkInkRenderer::Stats & LatkInkRenderer::getStats() const {
	return stats;
}

//--------------------------------------------------------------
void LatkInkRenderer::remodel(const Latk & latk, const glm::mat4 & matrix, const ofRectangle & viewport) {
	const uint64_t startTime = ofGetElapsedTimeMicros();
	stats = Stats();
	vertices.clear();
	colors.clear();
	indices.clear();

	// Strokes are cut where they leave the camera's depth range or a margin
	// around the viewport. Points close to the camera project to huge
	// coordinates, and modeling off-screen is wasted work. The margin keeps
	// the cut ends, where the modeled pen settles in, out of sight.
	ofRectangle bounds = viewport;
	bounds.scaleFromCenter(1.5);

	using EventType = ofxInkStrokeModeler::Input::EventType;
	const double interval = 1.0 / settings.inputRate;
	uint64_t projectMicros = 0, modelMicros = 0, meshMicros = 0;

	for (auto & layer : latk.layers) {
		if (layer.currentFrame < 0 || layer.currentFrame >= (int)layer.frames.size()) continue;

		for (auto & stroke : layer.frames[layer.currentFrame].strokes) {
			const size_t n = stroke.points.size();
			if (n == 0) continue;
			uint64_t time = ofGetElapsedTimeMicros();

			// The taper follows the whole stroke, so pieces of a cut stroke
			// keep their widths.
			pressures.resize(n);
			float length = 0;
			for (size_t i = 1; i < n; i++) length += stroke.points[i].distance(stroke.points[i - 1]);
			float distance = 0;
			for (size_t i = 0; i < n; i++) {
				if (i > 0) distance += stroke.points[i].distance(stroke.points[i - 1]);
				const float taper = ease(distance, settings.taperLength) * ease(length - distance, settings.taperLength);
				pressures[i] = ofLerp(settings.endPressure, 1, taper);
			}

			// LatkStroke::draw() scales its points by globalScale, so match it.
			screenPoints.resize(n);
			visible.assign(n, false);
			for (size_t i = 0; i < n; i++) {
				const glm::vec4 clip = matrix * glm::vec4(glm::vec3(stroke.points[i]) * stroke.globalScale, 1);
				if (clip.w <= 0 || std::abs(clip.z) > clip.w) continue;
				screenPoints[i] = {
					(clip.x / clip.w + 1) * 0.5f * viewport.width + viewport.x,
					(1 - clip.y / clip.w) * 0.5f * viewport.height + viewport.y,
				};
				visible[i] = bounds.inside(screenPoints[i]);
			}
			const ofFloatColor color = stroke.strokeColor;
			projectMicros += ofGetElapsedTimeMicros() - time;

			// Model each visible run of points as a stroke of its own.
			for (size_t first = 0; first < n;) {
				if (!visible[first]) {
					first++;
					continue;
				}
				size_t last = first;
				while (last + 1 < n && visible[last + 1]) last++;

				time = ofGetElapsedTimeMicros();
				bool ok = true;
				for (size_t i = first; i <= last && ok; i++) {
					const EventType type = i == first ? EventType::kDown : i < last ? EventType::kMove : EventType::kUp;
					ok = modeler.addInput(ofxInkStrokeModeler::makeInput(type, screenPoints[i], (i - first) * interval, pressures[i]));
				}
				if (ok && first == last) {
					// A single point: lift the pen where it touched down, for a dot.
					ok = modeler.addInput(ofxInkStrokeModeler::makeInput(EventType::kUp, screenPoints[first], interval, pressures[first]));
				}
				const uint64_t modeledTime = ofGetElapsedTimeMicros();
				modelMicros += modeledTime - time;

				if (ok) {
					const ofMesh mesh = ofxInkStrokeModeler::toMesh(modeler.getResults(), settings.width);
					const ofIndexType offset = vertices.size();
					vertices.insert(vertices.end(), mesh.getVertices().begin(), mesh.getVertices().end());
					colors.insert(colors.end(), mesh.getNumVertices(), color);
					for (ofIndexType index : mesh.getIndices()) indices.push_back(offset + index);

					stats.strokes++;
					stats.inputs += last - first + 1;
					stats.results += modeler.getResults().size();
				}
				meshMicros += ofGetElapsedTimeMicros() - modeledTime;
				first = last + 1;
			}
		}
	}

	const uint64_t uploadTime = ofGetElapsedTimeMicros();
	numIndices = indices.size();
	if (numIndices > 0) {
		vbo.setVertexData(vertices.data(), vertices.size(), GL_DYNAMIC_DRAW);
		vbo.setColorData(colors.data(), colors.size(), GL_DYNAMIC_DRAW);
		vbo.setIndexData(indices.data(), indices.size(), GL_DYNAMIC_DRAW);
	}

	stats.vertices = vertices.size();
	stats.triangles = indices.size() / 3;
	stats.projectMs = projectMicros / 1000.f;
	stats.modelMs = modelMicros / 1000.f;
	stats.meshMs = meshMicros / 1000.f;
	stats.uploadMs = millisSince(uploadTime);
	stats.totalMs = millisSince(startTime);
}
