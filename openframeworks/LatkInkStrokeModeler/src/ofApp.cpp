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
	// but the ink is projected by hand and never drawn inside it.
	cam.setEvents(ofEvents());

	ink.setup();
	ink.settings.inputRate = inputRates[inputRateIndex];
}

//--------------------------------------------------------------
void ofApp::update() {
	// Latk::run() advances and draws. Only advance here; draw() does the drawing.
	if (latk.checkInterval()) {
		for (auto & layer : latk.layers) layer.nextFrame();
	}
	latk.lastMillis = ofGetElapsedTimeMillis();
}

//--------------------------------------------------------------
void ofApp::draw() {
	ofBackground(0);
	string mode;
	if (drawInk) {
		// Remodels only if the camera or a frame changed since last time.
		ink.update(latk, cam);
		ink.draw();

		const auto & stats = ink.getStats();
		mode = "ink at " + ofToString(ink.settings.inputRate, 0) + " Hz (s to change) | last remodel "
			+ ofToString(stats.totalMs, 1) + " ms: " + ofToString(stats.inputs) + " points -> " + ofToString(stats.results);
	} else {
		cam.begin();
		// LatkStroke::draw() calls ofNoFill(), which would leave the ink
		// outlined. Restore the style afterwards.
		ofPushStyle();
		for (auto & layer : latk.layers) layer.run();
		ofPopStyle();
		cam.end();
		mode = "lines";
	}
	ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate(), 0) + " fps | " + mode + " | l: ink/lines", 10, 20);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == 'i') {
		// TODO
	}

	if (key == 'l') {
		drawInk = !drawInk;
	}

	if (key == 's') {
		inputRateIndex = (inputRateIndex + 1) % inputRates.size();
		ink.settings.inputRate = inputRates[inputRateIndex];
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
	// ofEasyCam takes its mouse area from the viewport it's begun with, and
	// it's never begun while drawing ink.
	cam.setControlArea(ofRectangle(0, 0, w, h));
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) { 

}
