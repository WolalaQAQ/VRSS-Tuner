#include "projection.h"
#include <cassert>
#include <iostream>
int main() {
    EyeGeometry eye{{-1,1,-1,1},{1,0,0,0,1,0,0,0,1}};
    Gain g;
    auto center=ProjectHeadTangent(0,0,eye,g,0);
    auto right=ProjectHeadTangent(.8,0,eye,g,0);
    auto up=ProjectHeadTangent(0,.8,eye,g,0);
    assert(std::abs(center[0])<1e-6 && std::abs(center[1])<1e-6);
    assert(std::abs(right[0]-.4)<1e-6 && std::abs(up[1]+.44)<1e-6);
    eye.projection={-1.4,1,-1,1};
    center=ProjectHeadTangent(0,0,eye,g,0);
    assert(std::abs(center[0]-1./6)<1e-6);
    const double a=.1;
    eye.eyeToHead={std::cos(a),0,std::sin(a),0,1,0,-std::sin(a),0,std::cos(a)};
    eye.projection={-1,1,-1,1};
    center=ProjectHeadTangent(0,0,eye,g,0);
    assert(std::abs(center[0]-std::tan(a))<1e-6);
    std::cout<<"PROJECTION_CENTER_GAIN_ASYMMETRIC_CANT_PASS\n";
}
