ofxHapPlayer
============

A Hap player for OpenFrameworks on macOS, Windows and Linux.

Hap is a codec for fast video playback. You can learn more about Hap, and find codecs for encoding, at the [main Hap project](https://github.com/Vidvox/hap).


Installation
------------

This repo has branches for major OF versions. Use the branch which matches the version of OF you are using. The [master](https://github.com/bangnoise/ofxHapPlayer/tree/master) branch matches the current OF release.

For example, if you want to use the addon with OpenFrameworks 0.9.x:

    $ cd addons/ofxHapPlayer
    $ git checkout OpenFrameworks-0.9


Linux Requirements
------------------

This step is only necessary on Linux. On macOS and Windows, the required libraries are bundled with the addon.

On Linux, ofxHapPlayer uses system libraries. For Ubuntu, the following packages are required:

libsnappy-dev, libswresample-dev, libavcodec-dev, libavformat-dev, libtbb-dev

    sudo apt-get install libsnappy-dev libswresample-dev libavcodec-dev libavformat-dev libtbb-dev

Pull-requests with instructions for other distributions are welcomed.

MSYS2 Requirements
------------------

ofxHapPlayer will use MSYS2-installed libraries. The following are required (assuming you are using the suggested MINGW64):

mingw-w64-x86_64-snappy, mingw-w64-x86_64-tbb, mingw-w64-x86_64-ffmpeg

    pacman -S mingw-w64-x86_64-snappy mingw-w64-x86_64-tbb mingw-w64-x86_64-ffmpeg

Some of these will usually have been installed as dependencies for OpenFrameworks.

Usage
-----

Use the OF Project Generator to generate build files for your project, selecting ofxHapPlayer as an addon.

    #import "ofxHapPlayer.h"

ofxHapPlayer inherits from ofBaseVideoPlayer

    player.loadMovie("movies/MyMovieName.mov");

When you want to draw:

	player.draw(20, 20);

Note that there is no direct access to pixels and calls to getPixels() will return NULL.

Advanced Usage
--------------

You can access the texture directly:

	ofTexture *texture = player.getTexture();

Note that if you access the texture directly for a Hap Q movie, you will need to use a shader when you draw:

    ofShader *shader = player.getShader();
    // the result of getShader() will be NULL if the movie is not Hap Q
    if (shader)
    {
        shader->begin();
    }
	texture.draw(x,y,w,h);
    if (shader)
    {
        shader->end();
    }

This is only necessary on desktop legacy contexts. On GLES (and desktop core
profiles), `player.draw()` owns the decode shader for every Hap format, so
prefer calling `draw()` and letting the addon handle it.

You can ask what a stream decodes to:

    unsigned int format = player.getHapTextureFormat();
    // one of HapTextureFormat_RGB_DXT1, HapTextureFormat_RGBA_DXT5,
    // HapTextureFormat_YCoCg_DXT5, or 0 if no Hap stream is loaded

OpenGL ES / Raspberry Pi
------------------------

GLES has no S3TC/DXT sampler and no `GL_BGRA`, so on GLES the addon uploads
the decoder's raw DXT blocks as an uncompressed RGBA "block plane" and expands
each 4x4 block with its own shader (GLSL ES 1.00). No S3TC-capable GPU is
required.

Check whether the device can run this before offering Hap sources:

    if (ofxHapPlayer::isHapSupported())
    {
        // offer Hap media
    }

This returns false on Raspberry Pi 1/2/3 (VideoCore IV) and on GLES contexts
without fragment `highp`. Pi 4/5 (V3D), desktop and platforms without a GPU
barrier return true.

Credits and License
-------------------

ofxHapPlayer was written by [Tom Butterworth](https://6a64.xyz), initially in April 2013, supported by [Igloo Vision](http://www.igloovision.com/) and James Sheridan. Since then it has been supported by [Vidvox](http://vidvox.net/). It is released under a [FreeBSD License](http://github.com/bangnoise/ofxHapPlayer/blob/master/LICENSE).
