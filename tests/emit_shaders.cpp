/*
 Writes every shader variant (all three dialects, all three formats) to a
 directory so an external validator such as glslangValidator can check them
 without a GPU. Used by `make -C tests shaders`.
*/

#include "ofxHapShaders.h"

#include <cstdio>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <output-dir>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];

    const ofxHapShaders::Dialect dialects[] = {
        ofxHapShaders::GLSL_ES_100, ofxHapShaders::GLSL_120, ofxHapShaders::GLSL_150
    };
    const char* dialectName[] = { "es100", "glsl120", "glsl150" };
    const unsigned int formats[] = {
        HapTextureFormat_RGB_DXT1, HapTextureFormat_RGBA_DXT5, HapTextureFormat_YCoCg_DXT5
    };
    const char* formatName[] = { "dxt1", "dxt5", "ycocg" };

    for (int d = 0; d < 3; ++d) {
        std::ofstream(dir + "/common_" + dialectName[d] + ".vert")
            << ofxHapShaders::vertexShader(dialects[d]);
        for (int f = 0; f < 3; ++f) {
            std::ofstream(dir + "/" + formatName[f] + "_" + dialectName[d] + ".frag")
                << ofxHapShaders::fragmentShader(dialects[d], formats[f]);
        }
    }
    return 0;
}
