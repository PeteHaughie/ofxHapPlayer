/*
 Tier-1 CPU tests for the pure helpers in src/ofxHapInternal.h.

 These build with a plain C++ compiler: no openFrameworks, no GL, no FFmpeg.
 Build and run with `make -C tests tier1`.
*/

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "../src/ofxHapInternal.h"

using namespace ofxHapInternal;

TEST_CASE("roundUpToMultipleOf4") {
    CHECK(roundUpToMultipleOf4(0) == 0);
    CHECK(roundUpToMultipleOf4(1) == 4);
    CHECK(roundUpToMultipleOf4(3) == 4);
    CHECK(roundUpToMultipleOf4(4) == 4);
    CHECK(roundUpToMultipleOf4(5) == 8);
    CHECK(roundUpToMultipleOf4(6) == 8);
    CHECK(roundUpToMultipleOf4(7) == 8);
    CHECK(roundUpToMultipleOf4(8) == 8);
    CHECK(roundUpToMultipleOf4(1920) == 1920);
}

TEST_CASE("bytesPerBlockForFormat") {
    CHECK(bytesPerBlockForFormat(HapTextureFormat_RGB_DXT1) == 8);
    CHECK(bytesPerBlockForFormat(HapTextureFormat_A_RGTC1) == 8);
    CHECK(bytesPerBlockForFormat(HapTextureFormat_RGBA_DXT5) == 16);
    CHECK(bytesPerBlockForFormat(HapTextureFormat_YCoCg_DXT5) == 16);
    CHECK(bytesPerBlockForFormat(0) == 0);
    CHECK(bytesPerBlockForFormat(0x1234) == 0);
}

TEST_CASE("textureFormatForCodecTag") {
    CHECK(textureFormatForCodecTag(fourcc('H', 'a', 'p', '1')) == HapTextureFormat_RGB_DXT1);
    CHECK(textureFormatForCodecTag(fourcc('H', 'a', 'p', '5')) == HapTextureFormat_RGBA_DXT5);
    CHECK(textureFormatForCodecTag(fourcc('H', 'a', 'p', 'Y')) == HapTextureFormat_YCoCg_DXT5);
    CHECK(textureFormatForCodecTag(fourcc('H', 'a', 'p', 'M')) == 0);
    CHECK(textureFormatForCodecTag(fourcc('a', 'v', 'c', '1')) == 0);
    CHECK(textureFormatForCodecTag(0) == 0);
}

TEST_CASE("blockPlaneFor geometry") {
    const auto dxt1 = blockPlaneFor(4, 4, HapTextureFormat_RGB_DXT1);
    CHECK(dxt1.valid);
    CHECK(dxt1.blockWidth == 1);
    CHECK(dxt1.blockHeight == 1);
    CHECK(dxt1.texelsPerBlock == 2);
    CHECK(dxt1.textureWidth == 2);
    CHECK(dxt1.textureHeight == 1);
    CHECK(dxt1.expectedBytes == 8);

    const auto dxt5 = blockPlaneFor(4, 4, HapTextureFormat_RGBA_DXT5);
    CHECK(dxt5.valid);
    CHECK(dxt5.texelsPerBlock == 4);
    CHECK(dxt5.textureWidth == 4);
    CHECK(dxt5.textureHeight == 1);
    CHECK(dxt5.expectedBytes == 16);

    // Non-multiple-of-4 dimensions are rounded up to whole blocks.
    const auto odd = blockPlaneFor(6, 6, HapTextureFormat_RGB_DXT1);
    CHECK(odd.blockWidth == 2);
    CHECK(odd.blockHeight == 2);
    CHECK(odd.textureWidth == 4);
    CHECK(odd.textureHeight == 2);
    CHECK(odd.expectedBytes == 32);

    const auto one = blockPlaneFor(1, 1, HapTextureFormat_YCoCg_DXT5);
    CHECK(one.valid);
    CHECK(one.textureWidth == 4);
    CHECK(one.textureHeight == 1);
    CHECK(one.expectedBytes == 16);

    const auto hd = blockPlaneFor(1920, 1080, HapTextureFormat_RGBA_DXT5);
    CHECK(hd.textureWidth == 1920);
    CHECK(hd.textureHeight == 270);
    CHECK(hd.expectedBytes == 2073600u);
}

TEST_CASE("blockPlaneFor rejects unsupported input") {
    CHECK_FALSE(blockPlaneFor(0, 4, HapTextureFormat_RGB_DXT1).valid);
    CHECK_FALSE(blockPlaneFor(4, 0, HapTextureFormat_RGB_DXT1).valid);
    CHECK_FALSE(blockPlaneFor(-4, 4, HapTextureFormat_RGB_DXT1).valid);
    CHECK_FALSE(blockPlaneFor(4, 4, 0).valid);
    CHECK_FALSE(blockPlaneFor(4, 4, 0x1234).valid);
}

TEST_CASE("blockPlane layout invariant: bytes == textureWidth*textureHeight*4") {
    const int sizes[][2] = { {1,1}, {4,4}, {8,4}, {4,8}, {6,6}, {7,9}, {16,16},
                             {1920,1080}, {3840,2160}, {3,5} };
    const unsigned int formats[] = { HapTextureFormat_RGB_DXT1,
                                     HapTextureFormat_RGBA_DXT5,
                                     HapTextureFormat_YCoCg_DXT5 };
    for (const auto& size : sizes) {
        for (unsigned int format : formats) {
            const auto plane = blockPlaneFor(size[0], size[1], format);
            REQUIRE(plane.valid);
            CHECK(plane.expectedBytes ==
                  static_cast<std::size_t>(plane.textureWidth) *
                  static_cast<std::size_t>(plane.textureHeight) * 4u);
        }
    }
}

TEST_CASE("blockPlaneMatches") {
    const auto plane = blockPlaneFor(4, 4, HapTextureFormat_RGBA_DXT5);
    CHECK(blockPlaneMatches(plane, 16));
    CHECK_FALSE(blockPlaneMatches(plane, 8));
    CHECK_FALSE(blockPlaneMatches(plane, 0));
    CHECK_FALSE(blockPlaneMatches(blockPlaneFor(0, 0, 0), 0));
}

TEST_CASE("hapSupportedForBoard") {
    SUBCASE("Pi 3 (VideoCore IV) is unsupported") {
        CHECK_FALSE(hapSupportedForBoard("Raspberry Pi 3 Model B Rev 1.2", "BCM2837"));
        CHECK_FALSE(hapSupportedForBoard("Raspberry Pi 2 Model B", "BCM2836"));
        CHECK_FALSE(hapSupportedForBoard("Raspberry Pi Zero W", "BCM2835"));
        CHECK_FALSE(hapSupportedForBoard("", "Hardware\t: BCM2835"));
    }
    SUBCASE("Pi 4 / Pi 5 (V3D) are supported") {
        CHECK(hapSupportedForBoard("Raspberry Pi 4 Model B Rev 1.4", "BCM2711"));
        CHECK(hapSupportedForBoard("Raspberry Pi 5 Model B Rev 1.0", "BCM2712"));
    }
    SUBCASE("unknown / non-Pi hardware defaults to supported") {
        CHECK(hapSupportedForBoard("", ""));
        CHECK(hapSupportedForBoard("Some Other Board", "Generic CPU"));
    }
}
