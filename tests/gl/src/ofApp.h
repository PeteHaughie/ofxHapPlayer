#pragma once

#include "ofMain.h"

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void draw() override;

private:
    bool runCase(unsigned int hapFormat, int width, int height, float tolerance);
    int failures = 0;
    int cases = 0;
};
