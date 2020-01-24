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

#include <util.hpp>
#include <volume.hpp>
#include <volume_samplers.hpp>

using std::make_unique;

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