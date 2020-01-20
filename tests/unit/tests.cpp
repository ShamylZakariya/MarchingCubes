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

auto approx(float v, float m = 1e-4F)
{
    return Approx(v).margin(m);
}

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
        REQUIRE(sampler.valueAt(vec3(radius, 0, 0), fuzz) == 0.0_a);

        // Point on inner fuzz boundary is inside
        REQUIRE(sampler.valueAt(vec3(radius - fuzz, 0, 0), fuzz) == 1_a);

        // Point halfway across fuzz boundary is half in/half out
        REQUIRE(sampler.valueAt(vec3(radius - fuzz / 2, 0, 0), fuzz) == 0.5_a);
    }

    SECTION("aabb")
    {
        // unit AABB intersects sphere
        REQUIRE(sampler.intersects(AABB(vec3(0, 0, 0), 1)));

        // tangential unit AABB intersects sphere
        REQUIRE(sampler.intersects(AABB(vec3(radius, 0, 0), 1)));

        // unit AABB outside sphere
        REQUIRE_FALSE(sampler.intersects(AABB(vec3(2 * radius, 0, 0), 1)));

        // enclosing AABB intersects sphere
        REQUIRE(sampler.intersects(AABB(vec3(0, 0, 0), 2 * radius)));

        // tangential AABB intersects sphere
        {
            auto bb = AABB(vec3(radius + 10, 0, 0), 10);
            auto r = sampler.intersects(bb);
            REQUIRE(r);
        }

        // distant AABB doesn't intersect sphere
        {
            auto bb = AABB(vec3(radius + 1000, 0, 0), 10);
            auto r = sampler.intersects(bb);
            REQUIRE_FALSE(r);
        }
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
        REQUIRE(sampler.intersects(AABB(vec3(0, 0, 0), 1)));

        //unit AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(1, 0, 0), 1)));

        //unit AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(-1, 0, 0), 1)));

        //unit AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(0, 0, 1), 1)));

        //unit AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(0, 0, -1), 1)));

        //tangential AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(0, 0.5, 0), 1)));

        //-tangential AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(0, -0.5, 0), 1)));

        //tangential AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(1, 0.5, 0), 1)));

        //-tangential AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(-1, -0.5, 0), 1)));

        //tangential AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(0, 0.5, 1), 1)));

        //-tangential AABB intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(0, -0.5, -1), 1)));

        //unit AABB at fuzziness boundary doesn't intersect bounded plane
        REQUIRE_FALSE(sampler.intersects(AABB(vec3(0, 1.5, 0), 1)));

        //unit AABB at -fuzziness boundary doesn't intersect bounded plane
        REQUIRE_FALSE(sampler.intersects(AABB(vec3(0, -1.5, 0), 1)));

        //Large straddling aabb intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(0, 0, 0), 10)));

        //Large aabb with enclosed corners intersects bounded plane
        REQUIRE(sampler.intersects(AABB(vec3(0, 4.75, 0), 10)));
    }
}

TEST_CASE("CubeSampler", "[samplers]")
{
    SECTION("Identity Transform Value At")
    {

        // cube from -1 -> +1 on each axis, centered at origin
        auto fuzz = 0;
        auto cube = RectangularPrismVolumeSampler(vec3(0), vec3(1), mat3 { 1 }, IVolumeSampler::Mode::Additive);

        // test axes
        for (float x = 0; x <= 2; x += 0.25F) {
            auto v = cube.valueAt(vec3(x, 0, 0), fuzz);
            REQUIRE(v == (x < 1 ? 1.0_a : 0.0_a));
        }
        for (float y = 0; y <= 2; y += 0.25F) {
            auto v = cube.valueAt(vec3(0, y, 0), fuzz);
            REQUIRE(v == (y < 1 ? 1.0_a : 0.0_a));
        }
        for (float z = 0; z <= 2; z += 0.25F) {
            auto v = cube.valueAt(vec3(0, 0, z), fuzz);
            REQUIRE(v == (z < 1 ? 1.0_a : 0.0_a));
        }
    }

    SECTION("Identity Transform Value At + Fuzz")
    {
        // cube from -1 -> +1 on each axis, centered at origin
        auto fuzz = 0.5;
        auto cube = RectangularPrismVolumeSampler(vec3(0), vec3(1), mat3 { 1 }, IVolumeSampler::Mode::Additive);

        auto eval = [&cube, &fuzz](vec3 p) {
            return cube.valueAt(p, fuzz);
        };

        REQUIRE(eval(vec3(0, 0, 0)) == approx(1.0F));
        REQUIRE(eval(vec3(0.5, 0, 0)) == approx(1.0F));
        REQUIRE(eval(vec3(-0.5, 0, 0)) == approx(1.0F));
        REQUIRE(eval(vec3(0.75, 0, 0)) == approx(0.5F));
        REQUIRE(eval(vec3(-0.75, 0, 0)) == approx(0.5F));
        REQUIRE(eval(vec3(1.0, 0, 0)) == approx(0.0F));
        REQUIRE(eval(vec3(-1.0, 0, 0)) == approx(0.0F));

        REQUIRE(eval(vec3(0, 0, 0)) == approx(1.0F));
        REQUIRE(eval(vec3(0, 0.5, 0)) == approx(1.0F));
        REQUIRE(eval(vec3(0, -0.5, 0)) == approx(1.0F));
        REQUIRE(eval(vec3(0, 0.75, 0)) == approx(0.5F));
        REQUIRE(eval(vec3(0, -0.75, 0)) == approx(0.5F));
        REQUIRE(eval(vec3(0, 1.0, 0)) == approx(0.0F));
        REQUIRE(eval(vec3(0, -1.0, 0)) == approx(0.0F));

        REQUIRE(eval(vec3(0, 0, 0)) == approx(1.0F));
        REQUIRE(eval(vec3(0, 0, 0.5)) == approx(1.0F));
        REQUIRE(eval(vec3(0, 0, -0.5)) == approx(1.0F));
        REQUIRE(eval(vec3(0, 0, 0.75)) == approx(0.5F));
        REQUIRE(eval(vec3(0, 0, -0.75)) == approx(0.5F));
        REQUIRE(eval(vec3(0, 0, 1.0)) == approx(0.0F));
        REQUIRE(eval(vec3(0, 0, -1.0)) == approx(0.0F));

        // now we should be at value 1 from -1 to +1 and ramp to 0 from +1->+2 and -1->-2
        cube.setSize(vec3(2, 2, 2));
        fuzz = 1.0F;
        for (float x = -3; x <= +3; x += 0.5F) {
            auto v = eval(vec3(x, 0, 0));
            auto expected = 1.0F;
            if (x >= +2 || x <= -2) {
                expected = 0;
            } else if (x > 1 && x < 2) {
                expected = 1 - (x - 1);
            } else if (x < -1 && x > -2) {
                expected = 1 - (fabs(x) - 1);
            }
            REQUIRE(v == approx(expected));
        }
    }

    SECTION("Translation Transform value at")
    {
        auto fuzz = 1;
        auto cube = RectangularPrismVolumeSampler(vec3(0), vec3(1), mat3 { 1 }, IVolumeSampler::Mode::Additive);

        auto eval = [&cube, &fuzz](vec3 p) {
            return cube.valueAt(p, fuzz);
        };

        auto test = [&eval, &cube](vec3 cubeOrigin) {
            // now we should be at value 1 from -1 to +1 and ramp to 0 from +1->+2 and -1->-2
            cube.setSize(vec3(2, 2, 2));
            cube.setPosition(cubeOrigin);

            for (float x = -3; x <= +3; x += 0.5F) {
                auto v = eval(cubeOrigin + vec3(x, 0, 0));
                auto expected = 1.0F;
                if (x >= +2 || x <= -2) {
                    expected = 0;
                } else if (x > 1 && x < 2) {
                    expected = 1 - (x - 1);
                } else if (x < -1 && x > -2) {
                    expected = 1 - (fabs(x) - 1);
                }
                REQUIRE(v == approx(expected));
            }

            for (float y = -3; y <= +3; y += 0.5F) {
                auto v = eval(cubeOrigin + vec3(0, y, 0));
                auto expected = 1.0F;
                if (y >= +2 || y <= -2) {
                    expected = 0;
                } else if (y > 1 && y < 2) {
                    expected = 1 - (y - 1);
                } else if (y < -1 && y > -2) {
                    expected = 1 - (fabs(y) - 1);
                }
                REQUIRE(v == approx(expected));
            }

            for (float z = -3; z <= +3; z += 0.5F) {
                auto v = eval(cubeOrigin + vec3(0, 0, z));
                auto expected = 1.0F;
                if (z >= +2 || z <= -2) {
                    expected = 0;
                } else if (z > 1 && z < 2) {
                    expected = 1 - (z - 1);
                } else if (z < -1 && z > -2) {
                    expected = 1 - (fabs(z) - 1);
                }
                REQUIRE(v == approx(expected));
            }
        };

        test(vec3(0, 0, 0));

        test(vec3(50, 0, 0));
        test(vec3(-50, 0, 0));

        test(vec3(0, 50, 0));
        test(vec3(0, -50, 0));

        test(vec3(0, 0, 50));
        test(vec3(0, 0, -50));
    }
}

TEST_CASE("AABB", "[aabb]")
{
    SECTION("AABBOctreeSubdivide")
    {
        auto bounds = iAABB(ivec3(0, 0, 0), ivec3(2, 2, 2));
        auto bounds_children = bounds.octreeSubdivide();

        REQUIRE(bounds_children.size() == 8);
        for (auto& b : bounds_children) {
            REQUIRE(b.size() == ivec3(1, 1, 1));
        }

        bounds = iAABB(ivec3(0, 0, 0), ivec3(8, 4, 2));
        bounds_children = bounds.octreeSubdivide();

        REQUIRE(bounds_children.size() == 8);
        auto expected = std::vector<iAABB> {
            iAABB(ivec3(0, 0, 0), ivec3(4, 2, 1)),
            iAABB(ivec3(4, 0, 0), ivec3(8, 2, 1)),
            iAABB(ivec3(4, 0, 1), ivec3(8, 2, 2)),
            iAABB(ivec3(0, 0, 1), ivec3(4, 2, 2)),

            iAABB(ivec3(0, 2, 0), ivec3(4, 4, 1)),
            iAABB(ivec3(4, 2, 0), ivec3(8, 4, 1)),
            iAABB(ivec3(4, 2, 1), ivec3(8, 4, 2)),
            iAABB(ivec3(0, 2, 1), ivec3(4, 4, 2))
        };

        auto bounds_children_v = std::vector<iAABB>(bounds_children.begin(), bounds_children.end());
        REQUIRE_THAT(bounds_children_v, Catch::Matchers::UnorderedEquals(expected));

        for (auto& b : bounds_children) {
            REQUIRE(b.size() == ivec3(4, 2, 1));
        }
    }
}

TEST_CASE("OctreePartitioning", "[octree]")
{
    SECTION("SimplePartitioning")
    {
        // 2x2x2 - should subdivide once to get leaf depth of 1 (2:0 -> 1:1) resulting in 9 (8^0 + 8^1) nodes
        const int size = 2;
        const int minSize = 1;
        const int leafDepth = 1;
        auto octree = OctreeVolume { size, 1, minSize, {} };

        int maxDepth = 0;
        int nodeCount = 0;
        octree.walkOctree([&maxDepth, &nodeCount, leafDepth, size](auto node) {
            maxDepth = max(maxDepth, node->depth);
            nodeCount++;

            if (node->isLeaf) {
                // Expect leaf node depth == 1
                REQUIRE(node->depth == leafDepth);
            } else {
                //Expect !leaf node depth < 1
                REQUIRE(node->depth < leafDepth);
            }

            auto bounds = node->bounds;
            int expectedSize = size >> node->depth;
            REQUIRE(bounds.size().x == expectedSize);
            REQUIRE(bounds.size().x == bounds.size().y);
            REQUIRE(bounds.size().x == bounds.size().z);

            return true;
        });

        // Expect octree node max depth == 2
        REQUIRE(maxDepth == leafDepth);

        // Expect node count to be (8^0 + 8^1)
        REQUIRE(nodeCount == 9);
    }
    SECTION("Partitioning")
    {
        // 16x16x16 - should get a max depth of 2 (16:0 -> 8:1 > 4:2) and 64 + 8 + 1 (8^0 + 8^1 + 8^2) nodes
        const int size = 16;
        const int minSize = 4;
        const int leafDepth = 2;
        auto octree = OctreeVolume { size, 1, minSize, {} };

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

            return true;
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
        auto octree = OctreeVolume { size, 1, minSize, {} };

        std::vector<OctreeVolume::Node*> nodes;
        octree.collect(nodes);

        // Expect empty octree not to result in any march callback invocations
        REQUIRE(nodes.empty());
    }

    SECTION("SimpleOctreeVolumeCulling")
    {
        // 2x2x2 - should halve once to get leaf depth of 1 (2:0 -> 1:1) resulting in 9 (8^0 + 8^1) nodes
        const int size = 2;
        const int minSize = 1;
        const int leafDepth = 1;

        auto octree = OctreeVolume { size, 0.1, minSize, {} };

        auto nodes = std::vector<OctreeVolume::Node*>();
        octree.walkOctree([&nodes](auto node) {
            nodes.push_back(node);
            return true;
        });

        REQUIRE(nodes.size() == 9);

        auto collect = [&octree]() -> auto
        {
            std::vector<OctreeVolume::Node*> nodes;
            octree.collect(nodes);
            return nodes;
        };

        auto sphere = octree.add(make_unique<SphereVolumeSampler>(vec3(0, 0, 0), 0.25F, IVolumeSampler::Mode::Additive));

        // small sphere at min position should land on one leaf node
        sphere->setPosition(vec3(0, 0, 0));
        sphere->setRadius(0.25F);
        auto result = collect();
        REQUIRE(result.size() == 1);
        REQUIRE(result[0]->isLeaf);

        // small sphere at center position should land on all eight leaf nodes,
        // resulting in coalescing up to 1 node at level 0
        sphere->setPosition(vec3(1, 1, 1));
        sphere->setRadius(0.5F);
        result = collect();
        REQUIRE(result.size() == 1);
        REQUIRE(result[0]->depth == 0);

        // small sphere at center bottom position should land on four leaf nodes
        sphere->setPosition(vec3(1, 0, 1));
        sphere->setRadius(0.25F);
        result = collect();
        REQUIRE(result.size() == 4);
        for (const auto& node : result) {
            REQUIRE(node->depth == leafDepth);
            REQUIRE(node->isLeaf);
        }
    }

    SECTION("OctreeVolumeCulling")
    {
        // 16x16x16 - should get a max depth of 2 (16:0 -> 8:1 > 4:2) and 64 + 8 + 1 (8^0 + 8^1 + 8^2) nodes
        const int size = 16;
        const int minSize = 4;
        const int leafDepth = 2;
        auto octree = OctreeVolume { size, 1, minSize, {} };
        auto sphere = octree.add(make_unique<SphereVolumeSampler>(vec3(0, 0, 0), 0.5F, IVolumeSampler::Mode::Additive));

        auto collect = [&octree]() -> auto
        {
            std::vector<OctreeVolume::Node*> nodes;
            octree.collect(nodes);
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