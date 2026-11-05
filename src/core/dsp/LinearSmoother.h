#pragma once

#include <algorithm>
#include <cmath>

namespace vs
{

// Linear ramp smoother. Real-time safe, header-only.
class LinearSmoother
{
public:
    void prepare (double sampleRate, double rampSeconds)
    {
        rampLength = std::max (1, (int) std::lround (sampleRate * rampSeconds));
        stepsLeft = 0;
    }

    void snapTo (float v) { current = target = v; stepsLeft = 0; }

    void setTarget (float v)
    {
        if (std::abs (v - target) <= 0.0f) // exact match, written to avoid -Wfloat-equal
            return;
        target = v;
        stepsLeft = rampLength;
        increment = (target - current) / (float) rampLength;
    }

    // Advance by n samples and return the value at the end of the advance.
    float advance (int n)
    {
        if (stepsLeft <= 0)
            return current = target;
        if (n >= stepsLeft)
        {
            stepsLeft = 0;
            return current = target;
        }
        stepsLeft -= n;
        current += increment * (float) n;
        return current;
    }

    float next() { return advance (1); }
    float getCurrent() const { return current; }
    float getTarget() const { return target; }
    bool isSmoothing() const { return stepsLeft > 0; }

private:
    float current = 0.0f, target = 0.0f, increment = 0.0f;
    int rampLength = 1, stepsLeft = 0;
};

} // namespace vs
