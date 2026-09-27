/*
 ofxHapInternal.h
 ofxHapPlayer

 Pure, dependency-free helpers shared by the player and its tests.

 Nothing in this header includes openFrameworks, FFmpeg or any GPU API: the
 functions below are deterministic and can be unit-tested by a plain C++
 test binary (see tests/). The GL/board-dependent pieces live in
 ofxHapPlayer.cpp behind these primitives.
*/

#ifndef __ofxHapInternal__
#define __ofxHapInternal__

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" {
#include <hap.h>
}

namespace ofxHapInternal {

    /*
     Round up to a multiple of 4 (DXT blocks are 4x4 pixels).
     */
    inline int roundUpToMultipleOf4(int n)
    {
        if (0 != (n & 3))
            n = (n + 3) & ~3;
        return n;
    }

    /*
     Little-endian four-character code, matching FFmpeg's MKTAG().
     */
    inline constexpr std::uint32_t fourcc(char a, char b, char c, char d)
    {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
    }

    /*
     Number of bytes in one 4x4 DXT block for a HapTextureFormat.
     Returns 0 for formats we cannot decode.
     */
    inline std::size_t bytesPerBlockForFormat(unsigned int format)
    {
        switch (format) {
            case HapTextureFormat_RGB_DXT1:  // fallthrough with A_RGTC1: both 8 bytes
            case HapTextureFormat_A_RGTC1:
                return 8;
            case HapTextureFormat_RGBA_DXT5:
            case HapTextureFormat_YCoCg_DXT5:
                return 16;
            default:
                return 0;
        }
    }

    /*
     Maps a stream codec_tag ('Hap1'/'Hap5'/'HapY') to the HapTextureFormat
     its frames decode to. Returns 0 when the tag is not a decodable Hap tag.
     */
    inline unsigned int textureFormatForCodecTag(std::uint32_t codecTag)
    {
        switch (codecTag) {
            case fourcc('H', 'a', 'p', '1'):
                return HapTextureFormat_RGB_DXT1;
            case fourcc('H', 'a', 'p', '5'):
                return HapTextureFormat_RGBA_DXT5;
            case fourcc('H', 'a', 'p', 'Y'):
                return HapTextureFormat_YCoCg_DXT5;
            default:
                return 0;
        }
    }

    /*
     Geometry of the uncompressed "block plane" texture used to upload raw DXT
     blocks on GLES (which has no S3TC). The decoder hands us the compressed
     plane as a flat run of little-endian 16-byte RGBA texels; we lay it out so
     the raw bytes upload verbatim with no CPU repack:

       - DXT5 / YCoCg : 16 bytes/block = 4 RGBA texels  -> one block spans
                        `texelsPerBlock` consecutive texels on a single row.
       - DXT1        :  8 bytes/block = 2 RGBA texels.

     Texture width  = (roundUp4(width) / 4) * (bytesPerBlock / 4)
     Texture height =  roundUp4(height) / 4

     This is exactly `roundUp4(width) * bytesPerBlock / 16` columns wide and
     touches only a quarter of the pixels, keeping dimensions well within the
     4096-texel GLES limits for real-world Hap resolutions.
     */
    struct BlockPlane
    {
        int         textureWidth;   // in RGBA8 texels
        int         textureHeight;  // in RGBA8 texels
        int         blockWidth;     // in 4x4 blocks
        int         blockHeight;    // in 4x4 blocks
        int         texelsPerBlock; // bytesPerBlock / 4
        std::size_t bytesPerBlock;
        std::size_t expectedBytes;  // decoder output size for this frame
        bool        valid;
    };

    inline BlockPlane blockPlaneFor(int width, int height, unsigned int format)
    {
        BlockPlane plane = {};
        const std::size_t bytesPerBlock = bytesPerBlockForFormat(format);
        if (bytesPerBlock == 0 || width <= 0 || height <= 0)
            return plane;

        plane.bytesPerBlock = bytesPerBlock;
        plane.texelsPerBlock = static_cast<int>(bytesPerBlock / 4);
        plane.blockWidth = roundUpToMultipleOf4(width) / 4;
        plane.blockHeight = roundUpToMultipleOf4(height) / 4;
        plane.textureWidth = plane.blockWidth * plane.texelsPerBlock;
        plane.textureHeight = plane.blockHeight;
        plane.expectedBytes = static_cast<std::size_t>(plane.blockWidth) *
                              static_cast<std::size_t>(plane.blockHeight) *
                              bytesPerBlock;
        plane.valid = true;
        return plane;
    }

    /*
     True when the decoded frame buffer has exactly the length implied by the
     plane geometry. A mismatch means either a malformed frame or a decoder
     surprise, and uploading it would read out of bounds.
     */
    inline bool blockPlaneMatches(const BlockPlane& plane, std::size_t decodedBytes)
    {
        return plane.valid && plane.expectedBytes == decodedBytes;
    }

    /*
     Board capability, expressed as a pure function over /proc data so it can
     be table-tested. The Raspberry Pi 1/2/3 (VideoCore IV) cannot run the
     block-decode shader; Pi 4/5 (V3D) can.
     */
    inline bool hapSupportedForBoard(const std::string& deviceTreeModel,
                                     const std::string& cpuHardware)
    {
        const std::string haystacks[] = { deviceTreeModel, cpuHardware };
        const char* unsupported[] = { "BCM2835", "BCM2836", "BCM2837" };
        for (const std::string& haystack : haystacks) {
            for (const char* token : unsupported) {
                if (haystack.find(token) != std::string::npos)
                    return false;
            }
        }
        return true;
    }

} // namespace ofxHapInternal

#endif /* defined(__ofxHapInternal__) */
