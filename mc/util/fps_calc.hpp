//
//  util.h
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef mc_fps_calc
#define mc_fps_calc

#include <epoxy/gl.h>
#include <limits>

#include <GLFW/glfw3.h>

namespace mc {
namespace util {

    class FpsCalculator {
    public:
        FpsCalculator() { reset(); }
        FpsCalculator(const FpsCalculator&) = delete;
        FpsCalculator(const FpsCalculator&&) = delete;
        ~FpsCalculator() = default;

        void reset()
        {
            _warmUp = true;
            _lastTimestamp = glfwGetTime();
            _fpsSum = 0;
            _fpsCount = 0;
            _averageFps = 0;
            _minFrameTime = std::numeric_limits<double>::max();
            _maxFrameTime = 0;
        }

        double update()
        {
            static constexpr int FPS_SAMPLES = 30;
            auto now = glfwGetTime();
            auto delta = now - _lastTimestamp;

            _fpsSum += 1.0F / delta;
            ++_fpsCount;

            if (_fpsCount == FPS_SAMPLES) {
                _averageFps = _fpsSum / _fpsCount;
                _fpsSum = 0;
                _fpsCount = 0;
                _minFrameTime = std::min(_minFrameTime, delta);
                _maxFrameTime = std::max(_minFrameTime, delta);
                _warmUp = false;
            } else if (_warmUp) {
                _averageFps = _fpsSum / _fpsCount;
            }

            _lastTimestamp = now;
            return delta;
        }

        double getFps() const { return _averageFps; }
        double getMinFrameTime() const { return _minFrameTime; }
        double getMaxFrameTime() const { return _maxFrameTime; }

    private:
        bool _warmUp = true;
        double _lastTimestamp = 0;
        double _fpsSum = 0;
        int _fpsCount = 0;
        double _averageFps = 0;
        double _minFrameTime = 0;
        double _maxFrameTime = 0;
    };

}
} // namespace mc::util

#endif /* mc_fps_calc */
