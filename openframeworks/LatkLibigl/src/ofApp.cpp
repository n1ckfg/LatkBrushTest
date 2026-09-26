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

	// Build every frame up front, so the animation never stalls on a new one.
	strokeMeshes.build(latk);
	ofLogNotice("ofApp") << "Built " << strokeMeshes.getNumFramesBuilt() << " frames of stroke meshes in "
		<< ofToString(strokeMeshes.getBuildSeconds(), 2) << " s: " << strokeMeshes.getNumTriangles() << " triangles, "
		<< ofToString(strokeMeshes.getNumBytes() / 1048576.0, 1) << " MB";

	// fractalNoiseTexture.frag reads its noise from this rather than hashing.
	strokeShader.setTexture("noiseTexture", StrokeShader::makeNoiseTexture());
	strokeShader.load(shaderPaths[shaderIndex]);
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
	cam.begin();
	if (drawTubes) {
		ofEnableDepthTest();
		// The tubes are closed, so their insides never show.
		glEnable(GL_CULL_FACE);
		strokeShader.begin();
		strokeMeshes.draw(latk);
		strokeShader.end();
		glDisable(GL_CULL_FACE);
		ofDisableDepthTest();
	} else {
		// LatkStroke::draw() calls ofNoFill(), which sets glPolygonMode(GL_LINE)
		// and would leave the tubes in wireframe. Restore the style afterwards.
		ofPushStyle();
		for (auto & layer : latk.layers) layer.run();
		ofPopStyle();
	}
	cam.end();

	const string mode = drawTubes ? "tubes, shader: " + strokeShader.getName() + " (s to change, r to reload)" : "lines";
	ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate(), 0) + " fps | " + mode + " | l: tubes/lines", 10, 20);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	if (key == 'i') {
		// TODO
	}

	if (key == 'l') {
		drawTubes = !drawTubes;
	}

	if (key == 's') {
		shaderIndex = (shaderIndex + 1) % shaderPaths.size();
		strokeShader.load(shaderPaths[shaderIndex]);
	}

	if (key == 'r') {
		strokeShader.reload();
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

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {

}
