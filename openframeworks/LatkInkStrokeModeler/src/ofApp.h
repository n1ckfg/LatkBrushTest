#pragma once

#include "ofMain.h"
#include "ofxLatk.h"
#include "LatkInkRenderer.h"

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
		LatkInkRenderer ink;

		// Input rates to cycle through with s: the faster, the smoother.
		vector<float> inputRates = { 90, 180, 360 };
		size_t inputRateIndex = 0;
		bool drawInk = true; // false draws Latk's original GL lines

};
