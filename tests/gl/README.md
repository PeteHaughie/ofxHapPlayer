# GPU decode test

> **Status: scaffold.** This compiles and runs, and has already caught real
> harness bugs, but the synthetic-plane comparison does not yet agree exactly
> with the CPU reference (a small, patterned set of pixels differs). The
> addon's decode algorithm itself is verified exactly by the CPU mirror in
> `tests/test_decode.cpp`, and every shader variant compiles under
> `glslangValidator` (`make -C tests shaders`). Treat this test as unfinished:
> it is not part of the required CI gate yet.

Renders a synthetic DXT1 / DXT5 / YCoCg block plane through the addon's
decode shaders (`src/ofxHapShaders.h`) into an FBO and compares every pixel
against the independent CPU reference decoder (`tests/reference_dxt.h`).

It does **not** go through `ofxHapPlayer`'s platform branching: the block
plane, texture and shader are set up exactly as the GLES upload path does, so
the same binary verifies GLSL ES 100 (GLES2/3), GLSL 120 (legacy desktop) and
GLSL 150 (core profile).

## Run

```sh
make -C tests/gl
bin/hap_gl_test        # exits non-zero on failure
```

On a headless Linux CI host wrap it in `xvfb-run`:

```sh
xvfb-run -a bin/hap_gl_test
```

On a Raspberry Pi the same test exercises the ES 100 shaders on the real GPU.
