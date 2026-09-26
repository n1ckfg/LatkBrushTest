#pragma once

#include "ofMain.h"
#include "ofxLatk.h"
#include "LatkMeshCache.h"
#include "StrokeShader.h"

class ofApp : public ofBaseApp {

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void mouseEntered(int x, int y);
		void mouseExited(int x, int y);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		Latk latk;
		ofEasyCam cam;
		LatkMeshCache strokeMeshes;
		StrokeShader strokeShader;

		// Fragment shaders to cycle through with s; "" is the built-in lit shader.
		vector<string> shaderPaths = { "shaders/fractalNoise.frag", "" };
		size_t shaderIndex = 0;
		bool drawTubes = true; // false draws Latk's original GL lines

};
