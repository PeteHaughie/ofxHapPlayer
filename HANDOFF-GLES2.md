# HANDOFF — GLES2 (Raspberry Pi) HAP playback

Status: **implemented and covered by tests; Pi hardware run still outstanding.**

This branch (`feat/gles2-texture`) makes the addon decode HAP on GLES (and
desktop core profiles), where there is no S3TC/DXT sampler and no `GL_BGRA`.

## Why this exists

The desktop path uploads HAP frames as compressed GPU textures:

```cpp
internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;   // Hap 1
internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;  // Hap 5 / Hap Y
_texture.allocate(texData, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
glCompressedTexSubImage2D(...);
```

None of those enums exist in GLES. Moving to `glesVersion = 3` does not help:
S3TC is not part of GLES at all, and the HAP source data is DXT, not ETC2/ASTC.

## What the addon now does

1. **Block plane upload (`getTexture`).** Under `TARGET_OPENGLES` the raw
   DXT1/DXT5/YCoCg blocks from `HapDecode()` are uploaded verbatim as an
   uncompressed RGBA8 texture, laid out so no CPU repack is needed:

   - `texelsPerBlock = bytesPerBlock / 4` (DXT1 = 2, DXT5/YCoCg = 4)
   - texture width  = `(roundUp4(w) / 4) * texelsPerBlock`
   - texture height = `roundUp4(h) / 4`

   Geometry, byte-budget validation and the codec-tag → `HapTextureFormat`
   mapping live in `src/ofxHapInternal.h` and are unit-tested.

2. **Addon-owned decode shaders (`src/ofxHapShaders.h`).** DXT1, DXT5 and
   YCoCg-DXT5 decoders written in float-only GLSL (GLSL ES 1.00 has no integer
   bit ops). Three dialects: `GLSL_ES_100` (GLES2/3), `GLSL_120` (legacy
   desktop), `GLSL_150` (desktop core). `getShader()` returns the right one and
   `draw()` renders a full-resolution quad through it on GLES/core; desktop
   DXT1/DXT5 keep the hardware S3TC path. The legacy desktop Hap Q shader is
   unchanged for GL<3.

3. **Accessors.** `getHapTextureFormat()` is the general form of `isHapQ()`
   (which is kept as a lock-guarded wrapper). `isHapSupported()` reports
   whether the device can decode Hap (board + fragment `highp`).

## What changed vs the earlier attempt

The earlier branch allocated an RGBA texture at `w/4 × h/4`, which is only the
right byte count for DXT5/YCoCg. DXT1 is 8 B/block, so the upload over-read
the buffer, and `getWidth()/4` under-uploaded non-multiple-of-4 frames. Both
are fixed, and the upload now refuses a frame whose decoded size does not match
the plane geometry.

## Tests

- `make -C tests all` — Tier 1 (geometry, byte budget, format mapping, board
  capability) and Tier 2 (a portable C++ mirror of the shader arithmetic
  compared against an independent reference DXT decoder, plus a block-plane
  addressing invariant). Runs in CI, including under ASan/UBSan.
- `make -C tests shaders` — emits every dialect/format and validates it with
  `glslangValidator`.
- `tests/gl/` — GPU FBO test that renders synthetic planes through the real
  shaders and compares to the CPU reference. **Scaffold / unfinished:** it
  compiles and runs but its synthetic-plane comparison does not yet match the
  CPU reference exactly, so it is not wired into the required CI gate. The
  decode algorithm is verified exactly by `tests/test_decode.cpp`; finishing
  this GPU test is a follow-up.
- The desktop example builds and links cleanly against a released OF.

## Still outstanding

- **Finish the GPU FBO test (`tests/gl`).** It compiles and already proved the
  shader geometry works, but its synthetic-plane comparison does not yet agree
  exactly with the CPU reference, so it is not in the CI gate. The decode math
  is covered exactly by the CPU mirror (`tests/test_decode.cpp`).
- **Pi 4 hardware run.** The GPU test (`tests/gl`) has not been run on a Pi
  yet; that is the one verification that cannot be done on a dev machine.
- **Pi 3 gate wiring.** `isHapSupported()` returns false on Pi 1/2/3 as
  intended; the consuming app (MoshBox) still needs to call it to exclude Hap
  sources. That is app-side work, tracked outside this addon.
- **HapM / Hap Q+A (two-texture frames).** Still unsupported, unchanged.

## Build context

Built into MoshBox's ARM64 artifact from
`.github/docker/linux-arm64.Dockerfile`, which clones this fork at a pinned SHA
(`OFX_HAPPLAYER_SHA`) and installs `libavformat-dev`, `libsnappy-dev`,
`libtbb-dev` (required by the addon's `linuxaarch64` config, which excludes the
bundled ffmpeg/snappy and links the system ones plus `-lsnappy`).
