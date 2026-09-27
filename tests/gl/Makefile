# GPU decode test. Needs a GL/GLES context (a display, or xvfb on CI).
#
#   make -C tests/gl && bin/hap_gl_test
#
# It only depends on the addon's pure shader/geometry headers, not the player,
# so it does not need FFmpeg.

OF_ROOT = ../../../..
PROJECT_CFLAGS = -I../../src -I../../libs/hap/src -I..

include $(OF_ROOT)/libs/openFrameworksCompiled/project/makefileCommon/compile.project.mk
