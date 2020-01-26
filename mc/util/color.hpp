//
//  util.h
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef mc_color_h
#define mc_color_h

#include <epoxy/gl.h>

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/norm.hpp>

namespace mc {
namespace util {
    namespace color {

        // cribbed from https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both

        struct rgb {
            float r; // a fraction between 0 and 1
            float g; // a fraction between 0 and 1
            float b; // a fraction between 0 and 1

            rgb()
                : r(0)
                , g(0)
                , b(0)
            {
            }

            rgb(float R, float G, float B)
                : r(R)
                , g(G)
                , b(B)
            {
            }

            explicit operator glm::vec3() const
            {
                return glm::vec3(r, g, b);
            }
        };

        struct hsv {
            float h; // angle in degrees (0,360)
            float s; // a fraction between 0 and 1
            float v; // a fraction between 0 and 1

            hsv()
                : h(0)
                , s(0)
                , v(0)
            {
            }

            hsv(float H, float S, float V)
                : h(H)
                , s(S)
                , v(V)
            {
            }
        };

        hsv Rgb2Hsv(rgb in);
        rgb Hsv2Rgb(hsv in);

    }
}
} // namespace mc::util::color

#endif /* mc_color_h */
