# HANDOFF — GLES2 (Raspberry Pi) HAP playback

Status: **compiles and boots, but HAP video does not render correctly yet.**
This branch (`feat/gles2-texture`, tip `3abadef`) makes the addon *build* for
`linuxaarch64` / `TARGET_OPENGLES`. It does not complete the feature.

## Why this branch exists

The stock addon's `getTexture()` uploads HAP frames as compressed GPU textures:

```cpp
internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;   // Hap 1
internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;  // Hap 5 / Hap Y
_texture.allocate(texData, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
glCompressedTexSubImage2D(...);
```

None of `GL_COMPRESSED_*_S3TC_*`, `GL_BGRA`, or
`GL_UNSIGNED_INT_8_8_8_8_REV` exist in GLES (2 **or** 3). They are desktop-GL
extension enums, so the file simply does not compile for the Pi. Moving the app
to `glesVersion = 3` does **not** help: S3TC is not part of GLES at all, and the
HAP source data is DXT, not ETC2/ASTC.

## What this branch changes

Two commits:

1. **`ofxHapPlayer.cpp` — uncompressed upload under `TARGET_OPENGLES`.**
   The HAP decoder (`HapDecode()` in `libs/hap`) still produces raw DXT1 / DXT5 /
   YCoCg-DXT5 blocks. On GLES the texture is allocated at **block resolution**
   (`w/4 × h/4`) as `GL_RGBA` / `GL_UNSIGNED_BYTE`, and the block bytes are
   uploaded with `glTexSubImage2D`. Desktop paths are untouched.
2. **`ofxHapPlayer.{h,cpp}` — `isHapQ()` accessor.** Returns true for a
   HapY stream. Callers use it to route Hap Q to their own YCoCg shader instead
   of the addon's bundled GLSL 120 one (which does not build on a core profile /
   GLES context).

## What is still missing (the actual feature)

The upload now hands the GPU a plane of packed DXT/YCoCg blocks. **Something must
expand those blocks in a fragment shader — that decoder is not written.**

- **Hap Q (`HapY`, YCoCg-DXT5)** — the consuming app (MoshBox) already has a
  GLES2 YCoCg shader (`bin/data/shaders/GLES2/hapq.*`) and routes to it via
  `isHapQ()`. This path should work.
- **Hap 1 / Hap 5 (DXT1 / DXT5)** — **no GLES decode shader exists.** They will
  render as 1/4-scale block garbage. Needs a `dxt1`/`dxt5` GLSL ES 1.00 shader
  that, given the block plane and the sub-block coordinate, decodes the 4×4
  block (565 colour endpoints + interpolation; DXT5 adds alpha).
- **No capability gating.** Nothing yet stops HAP sources being offered on a
  device that cannot run them (e.g. Pi 3 / vc4). Intended behaviour, per the
  MoshBox tags: **Pi 4+ and desktop offer HAP; Pi 3 is frozen to input devices.**
  Gate at runtime (board / `TARGET_RASPBERRY_PI` + GLES2), not by shipping a
  separate binary.

## Recommended next steps

1. Decide the accessor shape. `isHapQ()` is narrow; a
   `getHapTextureFormat()` returning `HapTextureFormat_{RGB_DXT1,RGBA_DXT5,YCoCg_DXT5}`
   (from `libs/hap/src/hap.h`) is more general and lets the caller pick DXT1 vs
   DXT5 vs YCoCg in one place. Prefer that before merging.
2. Write the DXT1/DXT5 decode shaders in the app (matching the existing
   `hapq.*` pattern — the app already owns shader-based decode).
3. Add the capability gate so Pi 3 excludes HAP sources.
4. Test on a Pi 4 with a real HAP file (`https://fate-suite.ffmpeg.org/hap/`).

## Build context

Built into MoshBox's ARM64 artifact from
`.github/docker/linux-arm64.Dockerfile`, which clones this fork at a pinned SHA
(`OFX_HAPPLAYER_SHA`) and installs `libavformat-dev`, `libsnappy-dev`,
`libtbb-dev` (required by the addon's `linuxaarch64` config, which excludes the
bundled ffmpeg/snappy and links the system ones plus `-lsnappy`).

## Verified

- Compiles and links for `linuxaarch64` (GLES2) via the MoshBox Docker build.
- Desktop (non-`TARGET_OPENGLES`) code paths are byte-for-byte unchanged.
- Not yet verified on hardware — see "What is still missing".
