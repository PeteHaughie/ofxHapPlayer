/*
 Tier-2 (CPU) decode tests.

 These mirror, in portable C++, exactly the arithmetic the GLSL decoders use
 (src/ofxHapShaders.h) and compare it against an independent reference DXT
 decoder. This catches byte-order, index-order and endpoint-ordering mistakes
 without needing a GPU. The GPU path itself is exercised by tests/gl/ on a
 machine with GL, and by the Pi hardware run.

 Build with `make -C tests tier2`.
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <cstdlib>
#include <vector>

#include "../src/ofxHapInternal.h"
#include "reference_dxt.h"

namespace {

using hapref::Pixel;
using hapref::decodeDXT1;
using hapref::decodeDXT5;

// ---- Mirror of the shader arithmetic (see ofxHapShaders.h) ----------------

int roundByte(double v) { return int(v < 0 ? 0 : (v > 255 ? 255 : v + 0.5)); }

int shader565Channel(int b0, int b1, int which) {
    int r5 = b1 / 8;
    int g6 = (b1 % 8) * 8 + b0 / 32;
    int b5 = b0 % 32;
    if (which == 0) return roundByte(r5 / 31.0 * 255.0);
    if (which == 1) return roundByte(g6 / 63.0 * 255.0);
    return roundByte(b5 / 31.0 * 255.0);
}

int ipow4(int sx) { int v = 1; for (int i = 0; i < sx; ++i) v *= 4; return v; }

Pixel shaderColour(const uint8_t* t0, int idx, bool c0Greater) {
    Pixel p;
    p.r = shader565Channel(t0[0], t0[1], 0);
    p.g = shader565Channel(t0[0], t0[1], 1);
    p.b = shader565Channel(t0[0], t0[1], 2);
    int r1 = shader565Channel(t0[2], t0[3], 0);
    int g1 = shader565Channel(t0[2], t0[3], 1);
    int b1 = shader565Channel(t0[2], t0[3], 2);
    if (idx == 0) return p;
    if (idx == 1) { p.r = r1; p.g = g1; p.b = b1; return p; }
    if (c0Greater) {
        if (idx == 2) { p.r = roundByte((2 * p.r + r1) / 3.0); p.g = roundByte((2 * p.g + g1) / 3.0); p.b = roundByte((2 * p.b + b1) / 3.0); }
        else          { p.r = roundByte((p.r + 2 * r1) / 3.0); p.g = roundByte((p.g + 2 * g1) / 3.0); p.b = roundByte((p.b + 2 * b1) / 3.0); }
    } else {
        if (idx == 2) { p.r = roundByte((p.r + r1) / 2.0); p.g = roundByte((p.g + g1) / 2.0); p.b = roundByte((p.b + b1) / 2.0); }
        else          { p.r = p.g = p.b = 0; }
    }
    return p;
}

bool endpointGreater565(int b0, int b1, int c0, int c1) {
    int ar = b1 / 8, ag = (b1 % 8) * 8 + b0 / 32, ab = b0 % 32;
    int br = c1 / 8, bg = (c1 % 8) * 8 + c0 / 32, bb = c0 % 32;
    if (ar != br) return ar > br;
    if (ag != bg) return ag > bg;
    return ab > bb;
}

std::vector<Pixel> shaderDecodeDXT1(const uint8_t* b) {
    std::vector<Pixel> out(16);
    for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
            int ib = b[4 + sy];
            int idx = (ib / ipow4(sx)) % 4;
            bool gt = endpointGreater565(b[0], b[1], b[2], b[3]);
            Pixel p = shaderColour(b, idx, gt);
            p.a = 255;
            out[sy * 4 + sx] = p;
        }
    }
    return out;
}

std::vector<Pixel> shaderDecodeDXT5(const uint8_t* b) {
    std::vector<Pixel> out(16);
    for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
            int ib = b[4 + sy];
            int idx = (ib / ipow4(sx)) % 4;
            bool gt = endpointGreater565(b[0], b[1], b[2], b[3]);
            Pixel p = shaderColour(b, idx, gt);
            // alpha, mirroring alphaIndex()/alphaForIndex()
            int p3 = 3 * (sy * 4 + sx);
            int abyte = p3 / 8, ashift = p3 % 8;
            // Alpha index bytes are b[10..15] (b[8],b[9] are the endpoints).
            // A 3-bit index can straddle a byte boundary, so combine two bytes.
            int lo = b[10 + abyte];
            int hi = (abyte < 5) ? b[10 + abyte + 1] : 0;
            int ai = ((lo + hi * 256) / (1 << ashift)) % 8;
            int a0 = b[8], a1 = b[9];
            int alpha;
            if (ai == 0) alpha = a0;
            else if (ai == 1) alpha = a1;
            else if (a0 > a1) alpha = roundByte(((8 - ai) * a0 + (ai - 1) * a1) / 7.0);
            else if (ai < 6)  alpha = roundByte(((6 - ai) * a0 + (ai - 1) * a1) / 5.0);
            else if (ai == 6) alpha = 0;
            else alpha = 255;
            p.a = alpha;
            out[sy * 4 + sx] = p;
        }
    }
    return out;
}

const int TOL = 3;

void checkClose(const std::vector<Pixel>& got, const std::vector<Pixel>& want, int tol) {
    REQUIRE(got.size() == want.size());
    for (std::size_t i = 0; i < got.size(); ++i) {
        CHECK(std::abs(got[i].r - want[i].r) <= tol);
        CHECK(std::abs(got[i].g - want[i].g) <= tol);
        CHECK(std::abs(got[i].b - want[i].b) <= tol);
        CHECK(std::abs(got[i].a - want[i].a) <= tol);
    }
}

} // namespace

TEST_CASE("shader DXT1 decode matches reference") {
    std::srand(12345);
    for (int trial = 0; trial < 500; ++trial) {
        uint8_t b[8];
        for (int i = 0; i < 8; ++i) b[i] = uint8_t(std::rand() & 0xFF);
        if (trial % 3 == 0) { // force the c0 > c1 branch
            uint16_t c0 = 0xF800, c1 = 0x001F;
            b[0] = c0 & 0xFF; b[1] = c0 >> 8; b[2] = c1 & 0xFF; b[3] = c1 >> 8;
        }
        if (trial % 3 == 1) { // force the c0 <= c1 branch
            uint16_t c0 = 0x001F, c1 = 0xF800;
            b[0] = c0 & 0xFF; b[1] = c0 >> 8; b[2] = c1 & 0xFF; b[3] = c1 >> 8;
        }
        checkClose(shaderDecodeDXT1(b), decodeDXT1(b), TOL);
    }
}

TEST_CASE("shader DXT5 decode matches reference") {
    std::srand(54321);
    for (int trial = 0; trial < 500; ++trial) {
        uint8_t b[16];
        for (int i = 0; i < 16; ++i) b[i] = uint8_t(std::rand() & 0xFF);
        if (trial % 3 == 0) { // a0 > a1
            b[8] = 255; b[9] = 0;
        }
        if (trial % 3 == 1) { // a0 <= a1
            b[8] = 0; b[9] = 255;
        }
        checkClose(shaderDecodeDXT5(b), decodeDXT5(b), TOL);
    }
}

TEST_CASE("block plane addressing lands on the right bytes") {
    // For every block and texel, the shader's fetch coordinate
    // (col = blockX*texelsPerBlock + j, row = blockY) must equal the flat
    // byte offset in the decoder's DXT buffer.
    const int sizes[][2] = { {4,4}, {8,4}, {6,6}, {1920,1080} };
    const unsigned int formats[] = { HapTextureFormat_RGB_DXT1, HapTextureFormat_RGBA_DXT5 };
    for (const auto& size : sizes) {
        for (unsigned int format : formats) {
            const auto plane = ofxHapInternal::blockPlaneFor(size[0], size[1], format);
            REQUIRE(plane.valid);
            for (int by = 0; by < plane.blockHeight; ++by) {
                for (int bx = 0; bx < plane.blockWidth; ++bx) {
                    for (int j = 0; j < plane.texelsPerBlock; ++j) {
                        const int col = bx * plane.texelsPerBlock + j;
                        const int row = by;
                        const std::size_t linearTexel =
                            std::size_t(row) * plane.textureWidth + col;
                        const std::size_t byteOffset =
                            (std::size_t(by) * plane.blockWidth + bx) * plane.bytesPerBlock +
                            std::size_t(j) * 4;
                        CHECK(linearTexel * 4 == byteOffset);
                    }
                }
            }
        }
    }
}
