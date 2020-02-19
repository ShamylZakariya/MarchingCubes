//
//  storage.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include <iostream>

#include "io.hpp"
#include "storage.hpp"

using namespace glm;

namespace mc {
namespace util {

    //
    // Vertex storage
    //

    void VertexP3C4::bindVertexAttributes()
    {
        glVertexAttribPointer(
            static_cast<GLuint>(AttributeLayout::Pos),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(VertexP3C4),
            (const GLvoid*)offsetof(VertexP3C4, pos));
        glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::Pos));

        glVertexAttribPointer(
            static_cast<GLuint>(AttributeLayout::Color),
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(VertexP3C4),
            (const GLvoid*)offsetof(VertexP3C4, color));
        glEnableVertexAttribArray(static_cast<GLuint>(AttributeLayout::Color));
    }

}
} // namespace mc::util