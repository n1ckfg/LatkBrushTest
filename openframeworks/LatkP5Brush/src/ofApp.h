#pragma once

#include "ofMain.h"
#include "ofxLatk.h"
#include "ofxP5Brush.h"

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

		void drawBrushStrokes();
		void drawSpline(const vector<glm::vec2> & points);

		Latk latk;
		ofEasyCam cam;
		ofxP5Brush brush;

		string brushName = "2B";
		float brushWeight = 1.5;
		float brushCurvature = 0.5;
		float minPointDistance = 2.0; // screen pixels between kept stroke points
		double brushSeed = 1;
		ofColor paperColor = ofColor::fromHex(0xf6f1e8);

		bool needsRedraw = true;
		glm::mat4 lastViewProjection;

};
