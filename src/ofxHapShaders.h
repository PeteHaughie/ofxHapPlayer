/*
 ofxHapShaders.h
 ofxHapPlayer

 GLSL sources for expanding a raw HAP block plane on the GPU.

 On GLES (and desktop core profiles) the addon uploads the decoder's raw
 DXT1 / DXT5 / YCoCg-DXT5 blocks as an uncompressed RGBA8 "block plane" (see
 ofxHapInternal::blockPlaneFor) and these shaders turn each 4x4 block back
 into pixels. Everything is done with float arithmetic only, because GLSL ES
 1.00 has no integer bit operations.

 The three output dialects are:
   GLSL_ES_100 : GLSL ES 1.00, used for GLES2 and GLES3 contexts
   GLSL_120    : desktop legacy profile
   GLSL_150    : desktop core profile (also the template for ES 3.00 callers)
*/

#ifndef __ofxHapShaders__
#define __ofxHapShaders__

#include <string>

extern "C" {
#include <hap.h>
}

namespace ofxHapShaders {

    enum Dialect {
        GLSL_ES_100,
        GLSL_120,
        GLSL_150
    };

    namespace detail {

        inline std::string replaceAll(std::string s, const std::string& from, const std::string& to)
        {
            std::size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.size(), to);
                pos += to.size();
            }
            return s;
        }

        inline std::string esPrecisionHeader()
        {
            return "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
                   "precision highp float;\n"
                   "#else\n"
                   "precision mediump float;\n"
                   "#endif\n";
        }

        // Helpers shared by every format. Body text uses texture2D/gl_FragColor
        // and is rewritten for the core-profile dialect below.
        static const char* fragmentHelpers =
            "uniform sampler2D hap_src;\n"
            "uniform vec2 hapTexSize;\n"
            "varying vec2 hapPixel;\n"
            "\n"
            "float channelByte(vec4 v, int k) {\n"
            "    float b;\n"
            "    if (k == 0) b = v.x;\n"
            "    else if (k == 1) b = v.y;\n"
            "    else if (k == 2) b = v.z;\n"
            "    else b = v.w;\n"
            "    return floor(b * 255.0 + 0.5);\n"
            "}\n"
            "\n"
            "vec4 fetchTexel(float col, float row) {\n"
            "    return texture2D(hap_src, (vec2(col, row) + 0.5) / hapTexSize);\n"
            "}\n"
            "\n"
            "vec3 decode565(float b0, float b1) {\n"
            "    float r = floor(b1 / 8.0) / 31.0;\n"
            "    float g = (mod(b1, 8.0) * 8.0 + floor(b0 / 32.0)) / 63.0;\n"
            "    float b = mod(b0, 32.0) / 31.0;\n"
            "    return vec3(r, g, b);\n"
            "}\n"
            "\n"
            "bool endpointGreater(float ab0, float ab1, float bb0, float bb1) {\n"
            "    float ar = floor(ab1 / 8.0);\n"
            "    float ag = mod(ab1, 8.0) * 8.0 + floor(ab0 / 32.0);\n"
            "    float ab = mod(ab0, 32.0);\n"
            "    float br = floor(bb1 / 8.0);\n"
            "    float bg = mod(bb1, 8.0) * 8.0 + floor(bb0 / 32.0);\n"
            "    float bb = mod(bb0, 32.0);\n"
            "    if (ar != br) return ar > br;\n"
            "    if (ag != bg) return ag > bg;\n"
            "    return ab > bb;\n"
            "}\n"
            "\n"
            "vec3 colourForIndex(vec3 c0, vec3 c1, float idx, bool c0Greater) {\n"
            "    if (idx < 0.5) return c0;\n"
            "    if (idx < 1.5) return c1;\n"
            "    if (c0Greater) {\n"
            "        if (idx < 2.5) return (2.0 * c0 + c1) / 3.0;\n"
            "        return (c0 + 2.0 * c1) / 3.0;\n"
            "    }\n"
            "    if (idx < 2.5) return (c0 + c1) * 0.5;\n"
            "    return vec3(0.0);\n"
            "}\n"
            "\n"
            "float alphaForIndex(float a0, float a1, float idx) {\n"
            "    if (idx < 0.5) return a0;\n"
            "    if (idx < 1.5) return a1;\n"
            "    if (a0 > a1) return ((8.0 - idx) * a0 + (idx - 1.0) * a1) / 7.0;\n"
            "    if (idx < 5.5) return ((6.0 - idx) * a0 + (idx - 1.0) * a1) / 5.0;\n"
            "    if (idx < 6.5) return 0.0;\n"
            "    return 255.0;\n"
            "}\n"
            "\n"
            "float colourIndexByte(vec4 indexTexel, int sy, int sx) {\n"
            "    float ib = channelByte(indexTexel, sy);\n"
            "    return mod(floor(ib / pow(4.0, float(sx))), 4.0);\n"
            "}\n"
            "\n"
            "float alphaIndex(vec4 t2, vec4 t3, int sy, int sx) {\n"
            "    float p3 = 3.0 * float(sy * 4 + sx);\n"
            "    float abyte = floor(p3 / 8.0);\n"
            "    float ashift = mod(p3, 8.0);\n"
            "    // A 3-bit index can straddle a byte boundary, so combine the low\n"
            "    // byte with the next one. The 16-bit pair is exact in highp.\n"
            "    float lo;\n"
            "    float hi = 0.0;\n"
            "    if (abyte < 0.5) { lo = channelByte(t2, 2); hi = channelByte(t2, 3); }\n"
            "    else if (abyte < 1.5) { lo = channelByte(t2, 3); hi = channelByte(t3, 0); }\n"
            "    else if (abyte < 2.5) { lo = channelByte(t3, 0); hi = channelByte(t3, 1); }\n"
            "    else if (abyte < 3.5) { lo = channelByte(t3, 1); hi = channelByte(t3, 2); }\n"
            "    else if (abyte < 4.5) { lo = channelByte(t3, 2); hi = channelByte(t3, 3); }\n"
            "    else { lo = channelByte(t3, 3); hi = 0.0; }\n"
            "    float pair = lo + hi * 256.0;\n"
            "    return mod(floor(pair / pow(2.0, ashift)), 8.0);\n"
            "}\n";

        // Colour-block decode shared by DXT1 and DXT5. Expects `t0` (endpoints)
        // and `indexTexel` already fetched.
        static const char* colourBlockMainPrefix =
            "    float px = floor(hapPixel.x);\n"
            "    float py = floor(hapPixel.y);\n"
            "    float bx = floor(px / 4.0);\n"
            "    float by = floor(py / 4.0);\n"
            "    int sx = int(mod(px, 4.0));\n"
            "    int sy = int(mod(py, 4.0));\n"
            "    float base = bx * HAP_TEXELS_PER_BLOCK;\n"
            "    vec4 t0 = fetchTexel(base, by);\n"
            "    vec4 t1 = fetchTexel(base + 1.0, by);\n"
            "    float b0 = channelByte(t0, 0);\n"
            "    float b1 = channelByte(t0, 1);\n"
            "    float b2 = channelByte(t0, 2);\n"
            "    float b3 = channelByte(t0, 3);\n"
            "    vec3 c0 = decode565(b0, b1);\n"
            "    vec3 c1 = decode565(b2, b3);\n"
            "    bool greater = endpointGreater(b0, b1, b2, b3);\n"
            "    float idx = colourIndexByte(t1, sy, sx);\n"
            "    vec3 col = colourForIndex(c0, c1, idx, greater);\n";

        inline std::string dxt1Main()
        {
            return std::string("#define HAP_TEXELS_PER_BLOCK 2.0\n")
                 + "void main() {\n"
                 + colourBlockMainPrefix
                 + "    gl_FragColor = vec4(col, 1.0);\n"
                 + "}\n";
        }

        inline std::string dxt5Main(bool ycocg)
        {
            std::string m = std::string("#define HAP_TEXELS_PER_BLOCK 4.0\n")
                 + "void main() {\n"
                 + colourBlockMainPrefix
                 + "    vec4 t2 = fetchTexel(base + 2.0, by);\n"
                 + "    vec4 t3 = fetchTexel(base + 3.0, by);\n"
                 + "    float a0 = channelByte(t2, 0);\n"
                 + "    float a1 = channelByte(t2, 1);\n"
                 + "    float alpha = alphaForIndex(a0, a1, alphaIndex(t2, t3, sy, sx));\n";
            if (ycocg) {
                m += "    vec4 cocgsy = vec4(col.r, col.g, col.b, alpha / 255.0);\n"
                     "    cocgsy += vec4(-0.50196078431373, -0.50196078431373, 0.0, 0.0);\n"
                     "    float scale = (cocgsy.z * (255.0 / 8.0)) + 1.0;\n"
                     "    float Co = cocgsy.x / scale;\n"
                     "    float Cg = cocgsy.y / scale;\n"
                     "    float Y = cocgsy.w;\n"
                     "    gl_FragColor = vec4(Y + Co - Cg, Y + Cg, Y - Co - Cg, 1.0);\n";
            } else {
                m += "    gl_FragColor = vec4(col, alpha / 255.0);\n";
            }
            m += "}\n";
            return m;
        }

        inline std::string fragmentBody(unsigned int hapFormat)
        {
            switch (hapFormat) {
                case HapTextureFormat_RGB_DXT1:
                    return dxt1Main();
                case HapTextureFormat_RGBA_DXT5:
                    return dxt5Main(false);
                case HapTextureFormat_YCoCg_DXT5:
                    return dxt5Main(true);
                default:
                    // Should never be built; callers must not request a shader
                    // for an unsupported format.
                    return "void main() { gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0); }\n";
            }
        }

    } // namespace detail

    inline std::string vertexShader(Dialect dialect)
    {
        switch (dialect) {
            case GLSL_ES_100:
                return "#version 100\n"
                       "precision highp float;\n"
                       "attribute vec4 position;\n"
                       "attribute vec2 texcoord;\n"
                       "uniform mat4 modelViewProjectionMatrix;\n"
                       "uniform vec2 hapVideoSize;\n"
                       "varying vec2 hapPixel;\n"
                       "void main() {\n"
                       "    hapPixel = texcoord * hapVideoSize;\n"
                       "    gl_Position = modelViewProjectionMatrix * position;\n"
                       "}\n";
            case GLSL_120:
                return "#version 120\n"
                       "attribute vec4 position;\n"
                       "attribute vec2 texcoord;\n"
                       "uniform mat4 modelViewProjectionMatrix;\n"
                       "uniform vec2 hapVideoSize;\n"
                       "varying vec2 hapPixel;\n"
                       "void main() {\n"
                       "    hapPixel = texcoord * hapVideoSize;\n"
                       "    gl_Position = modelViewProjectionMatrix * position;\n"
                       "}\n";
            case GLSL_150:
            default:
                return "#version 150\n"
                       "in vec4 position;\n"
                       "in vec2 texcoord;\n"
                       "uniform mat4 modelViewProjectionMatrix;\n"
                       "uniform vec2 hapVideoSize;\n"
                       "out vec2 hapPixel;\n"
                       "void main() {\n"
                       "    hapPixel = texcoord * hapVideoSize;\n"
                       "    gl_Position = modelViewProjectionMatrix * position;\n"
                       "}\n";
        }
    }

    inline std::string fragmentShader(Dialect dialect, unsigned int hapFormat)
    {
        std::string header;
        switch (dialect) {
            case GLSL_ES_100:
                header = std::string("#version 100\n") + detail::esPrecisionHeader();
                break;
            case GLSL_120:
                header = "#version 120\n";
                break;
            case GLSL_150:
            default:
                header = "#version 150\n"
                         "#define gl_FragColor hapColor\n"
                         "out vec4 hapColor;\n";
                break;
        }

        std::string body = std::string(detail::fragmentHelpers) + detail::fragmentBody(hapFormat);

        if (dialect == GLSL_150) {
            body = detail::replaceAll(body, "texture2D", "texture");
            body = detail::replaceAll(body, "varying", "in");
        }
        return header + body;
    }

} // namespace ofxHapShaders

#endif /* defined(__ofxHapShaders__) */
