#include "LatkInkRenderer.h"

#include <thread>

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
	return inputRate == other.inputRate && width == other.width && endPressure == other.endPressure && taperLength == other.taperLength
		&& minSpacing == other.minSpacing;
}

//--------------------------------------------------------------
bool LatkInkSettings::operator!=(const LatkInkSettings & other) const {
	return !(*this == other);
}

//--------------------------------------------------------------
void LatkInkRenderer::setup(size_t numThreads) {
	if (numThreads == 0) numThreads = std::max(1u, std::thread::hardware_concurrency());

	// The strokes are finished, so there's nothing to predict.
	auto params = ofxInkStrokeModeler::getDefaultParams(ofxInkStrokeModeler::Predictor::Disabled);
	// The modeler takes ceil(interval * min_output_rate) steps between inputs,
	// with the interval rounded to float. At input rates that divide 180 Hz,
	// that rounds up to a whole extra step: 3 instead of 2 at 90 Hz. A hair
	// under 180 Hz gives the intended count.
	params.sampling_params.min_output_rate *= 1 - 1e-5;

	workers.clear();
	workers.resize(numThreads);
	for (auto & worker : workers) worker.modeler.setup(params);
	invalidate();
}

//--------------------------------------------------------------
bool LatkInkRenderer::update(const Latk & latk, const ofCamera & cam, const ofRectangle & viewport) {
	if (workers.empty()) setup();

	// As ofCamera::worldToScreen() projects, but computed once for all points.
	glm::mat4 matrix = cam.getModelViewProjectionMatrix(viewport);
	if (cam.isVFlipped()) matrix = glm::scale(glm::mat4(1), glm::vec3(1, -1, 1)) * matrix;

	vector<int> frames;
	for (auto & layer : latk.layers) frames.push_back(layer.currentFrame);

	if (modeled && matrix == modeledMatrix && viewport == modeledViewport && frames == modeledFrames && settings == modeledSettings) {
		return false;
	}

	const uint64_t startTime = ofGetElapsedTimeMicros();
	stats = Stats();
	project(latk, matrix, viewport);
	stats.projectMs = millisSince(startTime);

	// Share the pieces out by number of points. The first share is modeled on
	// this thread.
	const uint64_t buildTime = ofGetElapsedTimeMicros();
	vector<size_t> ends(workers.size(), pieces.size());
	size_t share = 0, pointsShared = 0;
	for (size_t i = 0; i < pieces.size() && share + 1 < workers.size(); i++) {
		pointsShared += pieces[i].count;
		if (pointsShared * workers.size() >= points.size() * (share + 1)) ends[share++] = i + 1;
	}
	vector<std::thread> threads;
	for (size_t i = 1; i < workers.size(); i++) {
		if (ends[i - 1] == ends[i]) {
			build(workers[i], ends[i], ends[i]); // nothing to model, but clear the last ribbons
		} else {
			threads.emplace_back([this, i, &ends] { build(workers[i], ends[i - 1], ends[i]); });
		}
	}
	build(workers[0], 0, ends[0]);
	for (auto & thread : threads) thread.join();
	stats.buildMs = millisSince(buildTime);

	const uint64_t uploadTime = ofGetElapsedTimeMicros();
	upload();
	stats.uploadMs = millisSince(uploadTime);

	stats.strokes = pieces.size();
	stats.inputs = points.size();
	for (auto & worker : workers) {
		stats.results += worker.numResults;
		stats.vertices += worker.vertices.size();
		stats.triangles += worker.numIndices / 3;
		stats.modelMs += worker.modelMicros / 1000.f;
		stats.meshMs += worker.meshMicros / 1000.f;
	}
	stats.totalMs = millisSince(startTime);

	modeled = true;
	modeledMatrix = matrix;
	modeledViewport = viewport;
	modeledFrames = frames;
	modeledSettings = settings;
	return true;
}

//--------------------------------------------------------------
void LatkInkRenderer::draw() const {
	for (auto & worker : workers) {
		if (worker.numIndices > 0) worker.vbo.drawElements(GL_TRIANGLES, worker.numIndices);
	}
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
void LatkInkRenderer::project(const Latk & latk, const glm::mat4 & matrix, const ofRectangle & viewport) {
	pieces.clear();
	points.clear();
	pressures.clear();

	// Strokes are cut where they leave the camera's depth range or a margin
	// around the viewport. Points close to the camera project to huge
	// coordinates, and modeling off-screen is wasted work. The margin keeps
	// the cut ends, where the modeled pen settles in, out of sight.
	ofRectangle bounds = viewport;
	bounds.scaleFromCenter(1.5);

	for (auto & layer : latk.layers) {
		if (layer.currentFrame < 0 || layer.currentFrame >= (int)layer.frames.size()) continue;

		for (auto & stroke : layer.frames[layer.currentFrame].strokes) {
			const size_t n = stroke.points.size();
			if (n == 0) continue;

			// The taper follows the whole stroke, so pieces of a cut stroke
			// keep their widths.
			strokePressures.resize(n);
			float length = 0;
			for (size_t i = 1; i < n; i++) length += stroke.points[i].distance(stroke.points[i - 1]);
			float distance = 0;
			for (size_t i = 0; i < n; i++) {
				if (i > 0) distance += stroke.points[i].distance(stroke.points[i - 1]);
				const float taper = ease(distance, settings.taperLength) * ease(length - distance, settings.taperLength);
				strokePressures[i] = ofLerp(settings.endPressure, 1, taper);
			}

			// LatkStroke::draw() scales its points by globalScale, so match it.
			strokePoints.resize(n);
			strokeVisible.assign(n, false);
			for (size_t i = 0; i < n; i++) {
				const glm::vec4 clip = matrix * glm::vec4(glm::vec3(stroke.points[i]) * stroke.globalScale, 1);
				if (clip.w <= 0 || std::abs(clip.z) > clip.w) continue;
				strokePoints[i] = {
					(clip.x / clip.w + 1) * 0.5f * viewport.width + viewport.x,
					(1 - clip.y / clip.w) * 0.5f * viewport.height + viewport.y,
				};
				strokeVisible[i] = bounds.inside(strokePoints[i]);
			}

			for (size_t first = 0; first < n;) {
				if (!strokeVisible[first]) {
					first++;
					continue;
				}
				size_t end = first + 1;
				while (end < n && strokeVisible[end]) end++;

				Piece piece;
				piece.first = points.size();
				piece.count = end - first;
				piece.color = stroke.strokeColor;
				pieces.push_back(piece);
				points.insert(points.end(), strokePoints.begin() + first, strokePoints.begin() + end);
				pressures.insert(pressures.end(), strokePressures.begin() + first, strokePressures.begin() + end);
				first = end;
			}
		}
	}
}

//--------------------------------------------------------------
void LatkInkRenderer::build(Worker & worker, size_t firstPiece, size_t endPiece) {
	worker.vertices.clear();
	worker.colors.clear();
	worker.indices.clear();
	worker.modelMicros = 0;
	worker.meshMicros = 0;
	worker.numResults = 0;

	using EventType = ofxInkStrokeModeler::Input::EventType;
	const double interval = 1.0 / settings.inputRate;
	// Pressure isn't passed to the modeler: its stylus stage projects every
	// result back onto the input to interpolate it, which took almost as long
	// as the rest of the modeling. The inputs are evenly spaced in time, so
	// each result's pressure is looked up by its time instead. The pen trails
	// the input by mass x drag seconds when moving steadily, so look up that
	// much earlier.
	const auto & springParams = worker.modeler.getParams().position_modeler_params;
	const double lag = springParams.spring_mass_constant * springParams.drag_constant;

	const float minSpacingSquared = settings.minSpacing * settings.minSpacing;
	auto apart = [minSpacingSquared](const ofxInkStrokeModeler::Result & a, const ofxInkStrokeModeler::Result & b) {
		const float dx = a.position.x - b.position.x;
		const float dy = a.position.y - b.position.y;
		return dx * dx + dy * dy >= minSpacingSquared;
	};

	for (size_t p = firstPiece; p < endPiece; p++) {
		const Piece & piece = pieces[p];
		const glm::vec2 * piecePoints = points.data() + piece.first;
		const float * piecePressures = pressures.data() + piece.first;

		const uint64_t modelTime = ofGetElapsedTimeMicros();
		bool ok = true;
		for (size_t i = 0; i < piece.count && ok; i++) {
			const EventType type = i == 0 ? EventType::kDown : i + 1 < piece.count ? EventType::kMove : EventType::kUp;
			ok = worker.modeler.addInput(ofxInkStrokeModeler::makeInput(type, piecePoints[i], i * interval));
		}
		if (ok && piece.count == 1) {
			// A single point: lift the pen where it touched down, for a dot.
			ok = worker.modeler.addInput(ofxInkStrokeModeler::makeInput(EventType::kUp, piecePoints[0], interval));
		}
		const uint64_t meshTime = ofGetElapsedTimeMicros();
		worker.modelMicros += meshTime - modelTime;
		if (!ok) continue;

		auto width = [&](const ofxInkStrokeModeler::Result & result) {
			const float x = ofClamp((result.time.Value() - lag) / interval, 0, piece.count - 1);
			const size_t i = x;
			const float pressure = i + 1 < piece.count ? ofLerp(piecePressures[i], piecePressures[i + 1], x - i) : piecePressures[i];
			return settings.width * pressure;
		};
		// Skip results closer than minSpacing to the last one kept, but end
		// where the modeled stroke ends.
		const auto & results = worker.modeler.getResults();
		if (results.empty()) continue;
		worker.kept.clear();
		for (auto & result : results) {
			if (worker.kept.empty() || apart(result, worker.kept.back())) worker.kept.push_back(result);
		}
		if (!(worker.kept.back() == results.back())) {
			if (worker.kept.size() > 1 && !apart(results.back(), worker.kept.back())) worker.kept.pop_back();
			worker.kept.push_back(results.back());
		}

		const ofMesh mesh = ofxInkStrokeModeler::toMesh(worker.kept, width);
		const ofIndexType offset = worker.vertices.size();
		for (auto & vertex : mesh.getVertices()) worker.vertices.emplace_back(vertex);
		worker.colors.insert(worker.colors.end(), mesh.getNumVertices(), piece.color);
		for (ofIndexType index : mesh.getIndices()) worker.indices.push_back(offset + index);
		worker.numResults += results.size();
		worker.meshMicros += ofGetElapsedTimeMicros() - meshTime;
	}
}

//--------------------------------------------------------------
void LatkInkRenderer::upload() {
	for (auto & worker : workers) {
		worker.numIndices = worker.indices.size();
		if (worker.numIndices == 0) continue;
		worker.vbo.setVertexData(worker.vertices.data(), worker.vertices.size(), GL_DYNAMIC_DRAW);
		worker.vbo.setColorData(worker.colors.data(), worker.colors.size(), GL_DYNAMIC_DRAW);
		worker.vbo.setIndexData(worker.indices.data(), worker.indices.size(), GL_DYNAMIC_DRAW);
	}
}
