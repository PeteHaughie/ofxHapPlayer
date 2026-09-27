/*
 Tier-2 GPU decode test.

 Renders a synthetic DXT block plane through the addon's decode shaders
 (src/ofxHapShaders.h) into a texture and compares every pixel against the
 independent CPU reference decoder. This exercises the real GLSL and the
 block-plane addressing on any GL/GLES context, including a Pi.

 It uses raw GL (no OF renderer state, no FBO, no shader reflection) so the
 result is identical on desktop core profiles and GLES, and it runs headless.

 Run with `make -C tests/gl && bin/gl.app/Contents/MacOS/gl` (macOS) or the
 equivalent path elsewhere; exits non-zero on failure.
*/

#include "ofApp.h"

#include "ofxHapShaders.h"
#include "ofxHapInternal.h"
#include "reference_dxt.h"

#include <cstdint>
#include <cstdio>
#include <vector>
#include <cmath>

namespace {

std::uint32_t g_state = 0x12345678u;
std::uint8_t nextByte() {
    g_state = g_state * 1664525u + 1013904223u;
    return std::uint8_t(g_state >> 24);
}

ofxHapShaders::Dialect currentDialect() {
#if defined(TARGET_OPENGLES)
    return ofxHapShaders::GLSL_ES_100;
#else
    if (ofGetGLRenderer()->getGLVersionMajor() >= 3) {
        return ofxHapShaders::GLSL_150;
    }
    return ofxHapShaders::GLSL_120;
#endif
}

// A test-local vertex shader that generates the full-screen quad from
// gl_VertexID, so there is no attribute/VBO plumbing to get wrong. The
// fragment shader under test is the addon's real one.
std::string testVertexShader(ofxHapShaders::Dialect dialect) {
    const bool gl3 = (dialect == ofxHapShaders::GLSL_150);
    std::string s = gl3 ? "#version 150\n" : (dialect == ofxHapShaders::GLSL_120 ? "#version 120\n" : "#version 100\n");
    if (gl3) {
        s += "uniform vec2 hapVideoSize;\n"
             "out vec2 hapPixel;\n"
             "void main() {\n"
             "    float x = float((gl_VertexID << 1) & 2);\n"
             "    float y = float(gl_VertexID & 2);\n"
             "    hapPixel = vec2(x, y) * hapVideoSize;\n"
             "    gl_Position = vec4(x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0);\n"
             "}\n";
    } else {
        s += "attribute vec4 position;\n"
             "attribute vec2 texcoord;\n"
             "uniform mat4 modelViewProjectionMatrix;\n"
             "uniform vec2 hapVideoSize;\n"
             "varying vec2 hapPixel;\n"
             "void main() {\n"
             "    hapPixel = texcoord * hapVideoSize;\n"
             "    gl_Position = modelViewProjectionMatrix * position;\n"
             "}\n";
    }
    return s;
}

GLuint compile(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096] = {0};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        ofLogError("hap_gl_test") << "shader compile failed:\n" << log;
    }
    return ok ? shader : 0;
}

int clampi(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return int(v + 0.5f);
}

void ycocgReference(const hapref::Pixel& block, int& r, int& g, int& b) {
    float co = block.r / 255.0f - 0.50196078431373f;
    float cg = block.g / 255.0f - 0.50196078431373f;
    float sc = block.b / 255.0f;
    float y  = block.a / 255.0f;
    float scale = sc * (255.0f / 8.0f) + 1.0f;
    co /= scale;
    cg /= scale;
    r = clampi((y + co - cg) * 255.0f);
    g = clampi((y + cg) * 255.0f);
    b = clampi((y - co - cg) * 255.0f);
}

} // namespace

void ofApp::setup() {
    ofLogNotice("hap_gl_test") << "GL " << ofGetGLRenderer()->getGLVersionMajor()
                               << "." << ofGetGLRenderer()->getGLVersionMinor();
    // 52x37 is not a multiple of 4, so the last block row/column is padded.
    const int W = 52, H = 37;

    runCase(HapTextureFormat_RGB_DXT1, W, H, 3.0f);
    runCase(HapTextureFormat_RGBA_DXT5, W, H, 3.0f);
    runCase(HapTextureFormat_YCoCg_DXT5, W, H, 4.0f);

    ofLogNotice("hap_gl_test") << cases << " cases, " << failures << " failures";
    ofLogNotice("hap_gl_test") << (failures == 0 ? "PASS" : "FAIL");
    ofExit(failures == 0 ? 0 : 1);
}

void ofApp::draw() {}

bool ofApp::runCase(unsigned int hapFormat, int width, int height, float tolerance) {
    ++cases;
    const auto plane = ofxHapInternal::blockPlaneFor(width, height, hapFormat);
    if (!plane.valid) {
        ofLogError("hap_gl_test") << "invalid plane for format " << hapFormat;
        ++failures;
        return false;
    }

    const std::size_t bpb = plane.bytesPerBlock;
    std::vector<std::uint8_t> bytes(plane.expectedBytes);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = nextByte();
    }
    for (std::size_t blk = 0; blk * bpb + 4 <= bytes.size(); ++blk) {
        std::uint8_t* b = bytes.data() + blk * bpb;
        if (blk % 2 == 0) { b[0] = 0x00; b[1] = 0xF8; b[2] = 0x1F; b[3] = 0x00; } // c0 > c1
        else              { b[0] = 0x1F; b[1] = 0x00; b[2] = 0x00; b[3] = 0xF8; } // c0 < c1
        if (bpb == 16 && blk % 3 == 0) { b[8] = 255; b[9] = 0; }
        if (bpb == 16 && blk % 3 == 1) { b[8] = 0; b[9] = 255; }
    }

    // CPU reference image.
    std::vector<std::uint8_t> reference(std::size_t(width) * height * 4, 0);
    for (int by = 0; by < plane.blockHeight; ++by) {
        for (int bx = 0; bx < plane.blockWidth; ++bx) {
            const std::uint8_t* block = bytes.data() + (std::size_t(by) * plane.blockWidth + bx) * bpb;
            std::vector<hapref::Pixel> decoded =
                (hapFormat == HapTextureFormat_RGB_DXT1) ? hapref::decodeDXT1(block)
                                                         : hapref::decodeDXT5(block);
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const int x = bx * 4 + sx;
                    const int y = by * 4 + sy;
                    if (x >= width || y >= height) continue;
                    hapref::Pixel p = decoded[sy * 4 + sx];
                    std::uint8_t* dst = reference.data() + (std::size_t(y) * width + x) * 4;
                    if (hapFormat == HapTextureFormat_YCoCg_DXT5) {
                        int r, g, b;
                        ycocgReference(p, r, g, b);
                        dst[0] = std::uint8_t(r); dst[1] = std::uint8_t(g); dst[2] = std::uint8_t(b); dst[3] = 255;
                    } else {
                        dst[0] = std::uint8_t(p.r); dst[1] = std::uint8_t(p.g); dst[2] = std::uint8_t(p.b); dst[3] = std::uint8_t(p.a);
                    }
                }
            }
        }
    }

    // Block-plane texture: raw bytes as RGBA8, NEAREST.
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, plane.textureWidth, plane.textureHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());

    // Render target texture + FBO.
    GLuint outTex = 0;
    glGenTextures(1, &outTex);
    glBindTexture(GL_TEXTURE_2D, outTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        ofLogError("hap_gl_test") << "FBO incomplete";
        ++failures;
        return false;
    }

    const ofxHapShaders::Dialect dialect = currentDialect();
    GLuint vs = compile(GL_VERTEX_SHADER, testVertexShader(dialect));
    GLuint fs = compile(GL_FRAGMENT_SHADER, ofxHapShaders::fragmentShader(dialect, hapFormat));
    if (!vs || !fs) {
        ++failures;
        return false;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "position");
    glBindAttribLocation(prog, 1, "texcoord");
    glLinkProgram(prog);
    GLint linked = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[4096] = {0};
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        ofLogError("hap_gl_test") << "program link failed:\n" << log;
        ++failures;
        return false;
    }

    // Full-screen quad in NDC with texcoords flipped to match a top-left Y
    // origin, exactly as the addon's blit mesh does (texcoord (0,0) top-left).
    const GLfloat verts[] = {
        -1.0f, -1.0f,  1.0f, 0.0f,   // bottom-left  -> uv (1,0)
        -1.0f,  1.0f,  0.0f, 0.0f,   // top-left     -> uv (0,0)
         1.0f, -1.0f,  1.0f, 1.0f,   // bottom-right -> uv (1,1)
         1.0f,  1.0f,  0.0f, 1.0f,   // top-right    -> uv (0,1)
    };

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    GLuint vao = 0;
#ifndef TARGET_OPENGLES
    if (ofGetGLRenderer()->getGLVersionMajor() >= 3) {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
    }
#endif
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    // Identity model-view-projection: the quad is already in NDC.
    const GLfloat identity[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    glViewport(0, 0, width, height);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(prog, "hap_src"), 0);
    glUniformMatrix4fv(glGetUniformLocation(prog, "modelViewProjectionMatrix"), 1, GL_FALSE, identity);
    glUniform2f(glGetUniformLocation(prog, "hapVideoSize"), float(width), float(height));
    glUniform2f(glGetUniformLocation(prog, "hapTexSize"), float(plane.textureWidth), float(plane.textureHeight));
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Read back. GL rows are bottom-up; our reference is top-down.
    std::vector<std::uint8_t> readback(std::size_t(width) * height * 4, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());

    // The vertex shader places hapPixel.y = y (GL bottom-up), so readback row
    // 0 corresponds to reference row 0.
    int mismatches = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::uint8_t* got = readback.data() + (std::size_t(y) * width + x) * 4;
            const std::uint8_t* want = reference.data() + (std::size_t(y) * width + x) * 4;
            for (int c = 0; c < 4; ++c) {
                if (std::abs(int(got[c]) - int(want[c])) > tolerance) {
                    if (mismatches < 5) {
                        ofLogError("hap_gl_test") << "format " << hapFormat << " pixel (" << x << "," << y
                                                  << ") channel " << c << " got " << int(got[c])
                                                  << " want " << int(want[c]);
                    }
                    ++mismatches;
                }
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFbo);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
#ifndef TARGET_OPENGLES
    if (vao) glDeleteVertexArrays(1, &vao);
#endif
    glDeleteProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteTextures(1, &outTex);

    if (mismatches) {
        ofLogError("hap_gl_test") << "format " << hapFormat << ": " << mismatches << " channel mismatches";
        ++failures;
        return false;
    }
    ofLogNotice("hap_gl_test") << "format " << hapFormat << ": OK (" << width << "x" << height << ")";
    return true;
}
