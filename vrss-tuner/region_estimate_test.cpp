#include "RegionEstimate.h"
#include <iostream>
#include <limits>
#include <stdexcept>
static void Require(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
static bool Near(double a, double b, double tolerance=1e-8) { return std::abs(a-b) < tolerance; }
int main() {
    try {
        auto five = region::Calculate(5,1,4,1,0);
        Require(five.valid && five.supported && Near(five.visibleFraction,.05) && Near(five.shadingRatio,1.15), "5 percent MSAA4");
        Require(Near(five.radiusX*2, .252313252202016), "normalized diameter");
        Require(Near(region::Calculate(5,1,2,1,0).shadingRatio,1.05), "MSAA2");
        Require(Near(region::Calculate(5,1,8,1,0).shadingRatio,1.35), "MSAA8");
        auto wide = region::Calculate(5,2,4,1,0);
        Require(Near(wide.visibleFraction,.05) && Near(wide.radiusX/wide.radiusY,2), "aspect preserves unclipped area");
        Require(Near(region::Calculate(5,1,4,0,0).shadingRatio,1), "Off has no added shading");
        Require(region::Calculate(5,1,4,2,0).adaptive, "Adaptive explicitly marked");
        Require(!region::Calculate(5,1,4,3,0).supported && !region::Calculate(5,1,4,1,1).supported, "experimental layouts not estimated");
        auto full = region::Calculate(100,1,4,1,0);
        double r = std::sqrt(1/region::Pi);
        double segment = r*r*std::acos(.5/r)-.5*std::sqrt(r*r-.25);
        Require(Near(full.visibleFraction,1-4*segment,5e-5), "ellipse rectangle clipping analytical control");
        Require(Near(region::Calculate(100,2,4,1,0).visibleFraction,region::Calculate(100,.5,4,1,0).visibleFraction,5e-5), "rotated clipping symmetry");
        for (double invalid : {0.0,101.0,std::numeric_limits<double>::quiet_NaN()})
            Require(!region::Calculate(invalid,1,4,1,0).valid,"invalid inputs");
        std::cout << "REGION_ESTIMATE_PASS area sample_rates aspect off adaptive unsupported_layout clipping symmetry invalid_input\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
