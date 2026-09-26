#include "ofApp.h"

#include "Poco/StreamCopier.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/Zip/ZipStream.h"

#include <fstream>

namespace {

// A .latk file is a zip archive holding one .json document, but Latk::read()
// only parses plain JSON. Extract the document to a temp file and return its
// path, or return fileName unchanged if it isn't a zip.
string resolveLatkJson(const string & fileName) {
	std::ifstream in(ofToDataPath(fileName, true), std::ios::binary);
	char magic[2] = {0, 0};
	in.read(magic, 2);
	if (magic[0] != 'P' || magic[1] != 'K') return fileName;

	try {
		in.clear();
		in.seekg(0);
		Poco::Zip::ZipArchive archive(in);
		for (auto it = archive.headerBegin(); it != archive.headerEnd(); ++it) {
			if (!it->second.isFile() || ofToLower(ofFilePath::getFileExt(it->first)) != "json") continue;

			in.clear();
			Poco::Zip::ZipInputStream zipIn(in, it->second);
			const auto outPath = std::filesystem::temp_directory_path() / (ofFilePath::getBaseName(fileName) + ".json");
			std::ofstream out(outPath, std::ios::binary);
			Poco::StreamCopier::copyStream(zipIn, out);
			return outPath.string();
		}
		ofLogError("ofApp") << "No .json document found in " << fileName;
	} catch (const Poco::Exception & e) {
		ofLogError("ofApp") << "Unable to unzip " << fileName << ": " << e.displayText();
	}
	return fileName;
}

}

//--------------------------------------------------------------
void ofApp::setup() {
	latk = Latk(resolveLatkJson("jellyfish.latk"));

	// ofEasyCam normally hooks up its mouse and update listeners in begin(),
	// but the strokes are projected by hand and never drawn inside it.
	cam.setEvents(ofEvents());

	brush.setup(ofGetWidth(), ofGetHeight());
	brush.scaleBrushes(3);
}

//--------------------------------------------------------------
void ofApp::update() {
	// Latk::run() advances and draws with GL lines. Only advance here; the
	// strokes are drawn with the brush in drawBrushStrokes().
	if (latk.checkInterval()) {
		for (auto & layer : latk.layers) layer.nextFrame();
		needsRedraw = true;
	}
	latk.lastMillis = ofGetElapsedTimeMillis();
}

//--------------------------------------------------------------
void ofApp::draw() {
	// The brush canvas persists between frames, so only repaint it when the
	// Latk frame or the camera changes.
	const glm::mat4 viewProjection = cam.getModelViewProjectionMatrix();
	if (viewProjection != lastViewProjection) {
		lastViewProjection = viewProjection;
		needsRedraw = true;
	}
	if (needsRedraw) {
		drawBrushStrokes();
		needsRedraw = false;
	}

	brush.draw(0, 0);
	ofDrawBitmapStringHighlight("brush: " + brushName + " (b to change)", 10, 20);
}

//--------------------------------------------------------------
void ofApp::drawBrushStrokes() {
	// Brush pigments mix subtractively, so paint on paper rather than black.
	brush.clear(paperColor);
	// Reseed so the brush texture stays put while the camera moves.
	brush.seed(brushSeed);

	const ofRectangle viewport = ofGetCurrentViewport();
	// Stamping off-screen is wasted work, and points close to the camera
	// project to huge coordinates, so cut strokes at a margin around the window.
	ofRectangle bounds = viewport;
	bounds.scaleFromCenter(1.5);
	vector<glm::vec2> points;
	for (auto & layer : latk.layers) {
		if (layer.currentFrame < 0 || layer.currentFrame >= (int)layer.frames.size()) continue;

		for (auto & stroke : layer.frames[layer.currentFrame].strokes) {
			brush.set(brushName, stroke.strokeColor, brushWeight);
			points.clear();
			for (auto & p : stroke.points) {
				// LatkStroke::draw() scales its points by globalScale, so match it.
				glm::vec3 world = p * stroke.globalScale;
				glm::vec3 screen = cam.worldToScreen(world, viewport);
				if (screen.z < -1 || screen.z > 1 || !bounds.inside(screen.x, screen.y)) {
					// Outside the camera's view: break the stroke here.
					drawSpline(points);
					points.clear();
					continue;
				}
				if (!points.empty() && glm::distance(points.back(), glm::vec2(screen)) < minPointDistance) continue;
				points.emplace_back(screen);
			}
			drawSpline(points);
		}
	}
}

//--------------------------------------------------------------
void ofApp::drawSpline(const vector<glm::vec2> & points) {
	if (points.size() < 2) return;
	// Rounding a corner takes three points, so shorter strokes stay straight.
	brush.spline(points, points.size() < 3 ? 0 : brushCurvature);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == 'i') {
		// TODO
	}

	if (key == 'b') {
		const vector<string> names = brush.box();
		auto it = std::find(names.begin(), names.end(), brushName);
		brushName = (it == names.end() || std::next(it) == names.end()) ? names.front() : *std::next(it);
		needsRedraw = true;
	}

	if (key == 'o') {
		latk.write("test.latk");
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ) {

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
	cam.setControlArea(ofRectangle(0, 0, w, h));
	brush.setup(w, h);
	needsRedraw = true;
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) { 

}
