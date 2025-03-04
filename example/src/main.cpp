#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){
	ofGLESWindowSettings settings;
	settings.setGLESVersion(2);
	settings.windowMode = OF_GAME_MODE;
	settings.setSize(640, 480);
	ofCreateWindow(settings);
	ofRunApp(new ofApp());
}
