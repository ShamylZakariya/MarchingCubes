//
//  tests.cpp
//  Tests
//
//  Created by Shamyl Zakariya on 12/21/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <memory>

#include "src/util.hpp"
#include "src/volume.hpp"
#include "src/volume_samplers.hpp"

using std::make_unique;
using namespace Catch::literals;

TEST_CASE("SphereSampler", "[samplers]")
{
    auto pos = vec3(0, 0, 0);
    auto radius = 100.0F;
    auto fuzz = 1.0F;
    auto sampler = SphereVolumeSampler(pos, radius, IVolumeSampler::Mode::Additive);

    SECTION("valueAt")
    {
        // Point inside sphere is inside
        REQUIRE(sampler.valueAt(vec3(radius / 2, 0, 0), fuzz) == 1_a);

        // Point outside sphere is outside
        REQUIRE(sampler.valueAt(vec3(radius * 2, 0, 0), fuzz) == 0_a);

        // Point on boundary is outside
        REQUIRE(sampler.valueAt(vec3(radius, 0, 0), fuzz) == 0_a);

        // Point on inner fuzz boundary is inside
        REQUIRE(sampler.valueAt(vec3(radius - fuzz, 0, 0), fuzz) == 1_a);

        // Point halfway across fuzz boundary is half in/half out
        REQUIRE(sampler.valueAt(vec3(radius - fuzz / 2, 0, 0), fuzz) == 0.5_a);
    }

    SECTION("aabb")
    {
        // unit AABB intersects sphere
        REQUIRE(sampler.test(AABB(vec3(0, 0, 0), 1)));

        // tangential unit AABB intersects sphere
        REQUIRE(sampler.test(AABB(vec3(radius, 0, 0), 1)));

        // unit AABB outside sphere
        REQUIRE_FALSE(sampler.test(AABB(vec3(2 * radius, 0, 0), 1)));

        // enclosing AABB intersects sphere
        REQUIRE(sampler.test(AABB(vec3(0, 0, 0), 2 * radius)));

        // tangential enclosing AABB intersects sphere
        REQUIRE(sampler.test(AABB(vec3(radius, 0, 0), 2 * radius)));

        // distant AABB doesn't intersect sphere
        REQUIRE_FALSE(sampler.test(AABB(vec3(3 * radius, 0, 0), 2 * radius)));
    }
}

TEST_CASE("PlaneSampler", "[samplers]")
{
    // a simple plane on XZ facing up +Y
    // inside volume is from -0.5 to +0.5 on Y
    auto origin = vec3(0, 0, 0);
    auto normal = vec3(0, 1, 0);
    auto thickness = 1.0F;
    auto fuzz = 0.5F;
    auto sampler = BoundedPlaneVolumeSampler(origin, normal, thickness, IVolumeSampler::Mode::Additive);

    SECTION("valueAt")
    {
        //Point at origin of bounded plane is inside
        REQUIRE(sampler.valueAt(vec3(0, 0, 0), fuzz) == 1.0_a);

        //Point at halfway across fuzziness boundary is half-inside
        REQUIRE(sampler.valueAt(vec3(0, 0.25F, 0), fuzz) == 0.5_a);

        //Point at halfway across fuzziness boundary is half-inside
        REQUIRE(sampler.valueAt(vec3(0, -0.25F, 0), fuzz) == 0.5_a);

        //Point at fuzziness edge of bounded plane is outside
        REQUIRE(sampler.valueAt(vec3(0, 0.5F, 0), fuzz) == 0.0_a);

        //Point at fuzziness edge of bounded plane is outside
        REQUIRE(sampler.valueAt(vec3(0, -0.5F, 0), fuzz) == 0.0_a);

        //Point beyond fuzziness edge of bounded plane is outside
        REQUIRE(sampler.valueAt(vec3(0, 10.0F, 0), fuzz) == 0.0_a);

        //Point beyond fuzziness edge of bounded plane is outside
        REQUIRE(sampler.valueAt(vec3(0, -10.0F, 0), fuzz) == 0.0_a);
    }

    SECTION("aabb")
    {
        //unit AABB intersects bounded plane"
        REQUIRE(sampler.test(AABB(vec3(0, 0, 0), 1)));

        //unit AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(1, 0, 0), 1)));

        //unit AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(-1, 0, 0), 1)));

        //unit AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(0, 0, 1), 1)));

        //unit AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(0, 0, -1), 1)));

        //tangential AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(0, 0.5, 0), 1)));

        //-tangential AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(0, -0.5, 0), 1)));

        //tangential AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(1, 0.5, 0), 1)));

        //-tangential AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(-1, -0.5, 0), 1)));

        //tangential AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(0, 0.5, 1), 1)));

        //-tangential AABB intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(0, -0.5, -1), 1)));

        //unit AABB at fuzziness boundary doesn't intersect bounded plane
        REQUIRE_FALSE(sampler.test(AABB(vec3(0, 1.5, 0), 1)));

        //unit AABB at -fuzziness boundary doesn't intersect bounded plane
        REQUIRE_FALSE(sampler.test(AABB(vec3(0, -1.5, 0), 1)));

        //Large straddling aabb intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(0, 0, 0), 10)));

        //Large aabb with enclosed corners intersects bounded plane
        REQUIRE(sampler.test(AABB(vec3(0, 4.75, 0), 10)));
    }
}

TEST_CASE("OctreePartitioning", "[octree]")
{
    SECTION("Partitioning")
    {
        // 16x16x16 - should get a max depth of 2 (16:0 -> 8:1 > 4:2) and 64 + 8 + 1 (8^0 + 8^1 + 8^2) nodes
        const int size = 16;
        const int minSize = 4;
        const int leafDepth = 2;
        auto octree = OctreeVolume { size, 1, minSize };

        int maxDepth = 0;
        int nodeCount = 0;
        octree.walkOctree([&maxDepth, &nodeCount, leafDepth, size](auto node) {
            maxDepth = max(maxDepth, node->depth);
            nodeCount++;

            if (node->isLeaf) {
                // Expect leaf node depth == 2
                REQUIRE(node->depth == leafDepth);
            } else {
                //Expect !leaf node depth < 2
                REQUIRE(node->depth < leafDepth);
            }

            auto bounds = node->bounds;
            int expectedSize = size >> node->depth;
            REQUIRE(bounds.size().x == expectedSize);
            REQUIRE(bounds.size().x == bounds.size().y);
            REQUIRE(bounds.size().x == bounds.size().z);
        });

        // Expect octree node max depth == 2
        REQUIRE(maxDepth == 2);

        // Expect node count to be (8^0 + 8^1 + 8^2)
        REQUIRE(nodeCount == 73);
    }

    SECTION("EmptyOctreeVolumeCulling")
    {
        // 16x16x16 - should get a max depth of 2 (16:0 -> 8:1 > 4:2) and 64 + 8 + 1 (8^0 + 8^1 + 8^2) nodes
        const int size = 16;
        const int minSize = 4;
        auto octree = OctreeVolume { size, 1, minSize };

        int called = 0;
        octree.visitOccupiedNodes(true, [&called](auto node) {
            called++;
        });

        // Expect empty octree not to result in any march callback invocations
        REQUIRE(called == 0);
    }

    SECTION("SimpleOctreeVolumeCulling")
    {
        // 16x16x16 - should get a max depth of 2 (16:0 -> 8:1 > 4:2) and 64 + 8 + 1 (8^0 + 8^1 + 8^2) nodes
        const int size = 16;
        const int minSize = 4;
        const int leafDepth = 2;
        auto octree = OctreeVolume { size, 1, minSize };
        auto sphere = octree.add(make_unique<SphereVolumeSampler>(vec3(0, 0, 0), 0.5F, IVolumeSampler::Mode::Additive));

        auto collect = [&octree]() -> auto
        {
            std::vector<OctreeVolume::Node*> nodes;
            octree.visitOccupiedNodes(true, [&nodes](auto node) {
                nodes.push_back(node);
            });
            return nodes;
        };

        // small sphere at min position of octree should land on ONE leaf node
        sphere->setPosition(vec3(0, 0, 0));
        sphere->setRadius(0.5F);
        auto result = collect();
        REQUIRE(result.size() == 1);
        REQUIRE(result[0]->isLeaf);

        // small sphere at x = 4 and z = 4, but y = 0 should collect 4 leaf nodes
        sphere->setPosition(vec3(minSize, 0, minSize));
        result = collect();
        REQUIRE(result.size() == 4);
        for (auto node : result) {
            REQUIRE(node->isLeaf);
            REQUIRE(node->depth == leafDepth);
        }

        // small sphere at x = 4 and z = 4, and y = 4 should collect 1 node at depth of 1
        // because it's in all 8 leaf nodes
        sphere->setPosition(vec3(minSize, minSize, minSize));
        result = collect();
        REQUIRE(result.size() == 1);
        REQUIRE(result[0]->depth == leafDepth - 1);
    }
}