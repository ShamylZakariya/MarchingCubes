//
//  util.h
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#ifndef mc_util_h
#define mc_util_h

#include <limits>

#include <epoxy/gl.h>

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/norm.hpp>

#include "aabb.hpp"
#include "color.hpp"
#include "io.hpp"
#include "lines.hpp"
#include "storage.hpp"
#include "thread_pool.hpp"
#include "unowned_ptr.hpp"

#endif /* mc_util_h */
