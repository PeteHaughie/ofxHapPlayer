#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofGLWindowSettings settings;
    // A core profile exercises the GLSL_150 dialect; the default (2.1) uses
    // GLSL_120. On GLES builds the ES_100 dialect is used.
#if !defined(TARGET_OPENGLES)
    settings.setGLVersion(3, 2);
#endif
    settings.setSize(320, 240);
    settings.windowMode = OF_WINDOW;
    ofCreateWindow(settings);

    ofRunApp(new ofApp());
    return 0;
}
