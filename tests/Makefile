# Tier-1 CPU tests for ofxHapInternal.h. No openFrameworks / GL / FFmpeg needed.
#
#   make -C tests tier1         build and run
#   make -C tests tier1-asan    same, under ASan + UBSan
#   make -C tests clean

CXX ?= c++
CXXFLAGS ?= -std=c++11 -O2 -Wall -Wextra -Werror
SANFLAGS := -std=c++11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
INCLUDES := -I../src -I../libs/hap/src

.PHONY: all tier1 tier2 all-asan shaders clean

all: tier1 tier2

tier1: test_internal
	./test_internal

tier2: test_decode
	./test_decode

all-asan: test_internal-asan test_decode-asan
	./test_internal-asan
	./test_decode-asan

test_internal: test_internal.cpp ../src/ofxHapInternal.h doctest.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) test_internal.cpp -o $@

test_decode: test_decode.cpp ../src/ofxHapInternal.h doctest.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) test_decode.cpp -o $@

# Validate every shader variant with glslangValidator if it is installed.
shaders: emit_shaders
	mkdir -p shaders_out
	./emit_shaders shaders_out
	for f in shaders_out/*.vert shaders_out/*.frag; do glslangValidator "$$f" || exit 1; done
	@echo "all shaders validate"

emit_shaders: emit_shaders.cpp ../src/ofxHapShaders.h ../src/ofxHapInternal.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) emit_shaders.cpp -o $@

test_internal-asan: test_internal.cpp ../src/ofxHapInternal.h doctest.h
	$(CXX) $(SANFLAGS) $(INCLUDES) test_internal.cpp -o $@

test_decode-asan: test_decode.cpp ../src/ofxHapInternal.h doctest.h
	$(CXX) $(SANFLAGS) $(INCLUDES) test_decode.cpp -o $@

clean:
	rm -f test_internal test_decode test_internal-asan test_decode-asan emit_shaders
	rm -rf shaders_out
