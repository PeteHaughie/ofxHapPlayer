/*
 reference_dxt.h
 Spec-literal CPU reference decoders for DXT1 / DXT5, shared by the CPU
 mirror tests (test_decode.cpp) and the GPU FBO tests (tests/gl/).
*/

#ifndef __ofxHapReferenceDxt__
#define __ofxHapReferenceDxt__

#include <cstdint>
#include <vector>

namespace hapref {

    struct Pixel { int r, g, b, a; };

    inline void expand565(std::uint16_t c, int& r, int& g, int& b) {
        int r5 = (c >> 11) & 0x1F;
        int g6 = (c >> 5) & 0x3F;
        int b5 = c & 0x1F;
        r = (r5 << 3) | (r5 >> 2);
        g = (g6 << 2) | (g6 >> 4);
        b = (b5 << 3) | (b5 >> 2);
    }

    inline void refColour(std::uint16_t c0, std::uint16_t c1, int idx, Pixel& out) {
        int r0, g0, b0, r1, g1, b1;
        expand565(c0, r0, g0, b0);
        expand565(c1, r1, g1, b1);
        if (idx == 0) { out.r = r0; out.g = g0; out.b = b0; return; }
        if (idx == 1) { out.r = r1; out.g = g1; out.b = b1; return; }
        if (c0 > c1) {
            if (idx == 2) { out.r = (2 * r0 + r1) / 3; out.g = (2 * g0 + g1) / 3; out.b = (2 * b0 + b1) / 3; }
            else          { out.r = (r0 + 2 * r1) / 3; out.g = (g0 + 2 * g1) / 3; out.b = (b0 + 2 * b1) / 3; }
        } else {
            if (idx == 2) { out.r = (r0 + r1) / 2; out.g = (g0 + g1) / 2; out.b = (b0 + b1) / 2; }
            else          { out.r = out.g = out.b = 0; }
        }
    }

    // Hap 1 is RGB_DXT1 (no 1-bit alpha): every sample is opaque, matching
    // GL_COMPRESSED_RGB_S3TC_DXT1_EXT on desktop.
    inline std::vector<Pixel> decodeDXT1(const std::uint8_t* b) {
        std::uint16_t c0 = std::uint16_t(b[0] | (b[1] << 8));
        std::uint16_t c1 = std::uint16_t(b[2] | (b[3] << 8));
        std::uint32_t bits = std::uint32_t(b[4] | (b[5] << 8) | (b[6] << 16) | (std::uint32_t(b[7]) << 24));
        std::vector<Pixel> out(16);
        for (int p = 0; p < 16; ++p) {
            refColour(c0, c1, int((bits >> (2 * p)) & 3), out[p]);
            out[p].a = 255;
        }
        return out;
    }

    inline std::vector<Pixel> decodeDXT5(const std::uint8_t* b) {
        std::uint16_t c0 = std::uint16_t(b[0] | (b[1] << 8));
        std::uint16_t c1 = std::uint16_t(b[2] | (b[3] << 8));
        std::uint32_t cbits = std::uint32_t(b[4] | (b[5] << 8) | (b[6] << 16) | (std::uint32_t(b[7]) << 24));
        int a0 = b[8], a1 = b[9];
        std::uint64_t abits = 0;
        for (int i = 0; i < 6; ++i) abits |= std::uint64_t(b[10 + i]) << (8 * i);

        std::vector<Pixel> out(16);
        for (int p = 0; p < 16; ++p) {
            refColour(c0, c1, int((cbits >> (2 * p)) & 3), out[p]);
            int ai = int((abits >> (3 * p)) & 7);
            int alpha;
            if (ai == 0) alpha = a0;
            else if (ai == 1) alpha = a1;
            else if (a0 > a1) alpha = ((8 - ai) * a0 + (ai - 1) * a1) / 7;
            else if (ai < 6) alpha = ((6 - ai) * a0 + (ai - 1) * a1) / 5;
            else if (ai == 6) alpha = 0;
            else alpha = 255;
            out[p].a = alpha;
        }
        return out;
    }

} // namespace hapref

#endif /* defined(__ofxHapReferenceDxt__) */
