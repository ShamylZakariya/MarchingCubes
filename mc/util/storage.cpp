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
    // Vertex
    //

    void Vertex::bindVertexAttributes()
    {
        glVertexAttribPointer(
            static_cast<GLuint>(Vertex::AttributeLayout::Pos),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Pos));

        glVertexAttribPointer(
            static_cast<GLuint>(Vertex::AttributeLayout::Color),
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, color));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Color));

        glVertexAttribPointer(
            static_cast<GLuint>(Vertex::AttributeLayout::Normal),
            3,
            GL_FLOAT,
            GL_FALSE,
            sizeof(Vertex),
            (const GLvoid*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(static_cast<GLuint>(Vertex::AttributeLayout::Normal));
    }

}
} // namespace mc::util