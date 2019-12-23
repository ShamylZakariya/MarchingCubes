//
//  Tests.m
//  Tests
//
//  Created by Shamyl Zakariya on 12/21/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#import <memory>

#import "volume.hpp"
#import "volume_samplers.hpp"

#import <XCTest/XCTest.h>

using std::make_unique;
constexpr auto EPSILON = 1e-6F;

@interface Tests : XCTestCase

@end

@implementation Tests

- (void)testSphereSampler
{
    auto pos = vec3(0, 0, 0);
    auto radius = 100.0F;
    auto fuzz = 1.0F;
    auto sampler = SphereVolumeSampler(pos, radius, IVolumeSampler::Mode::Additive);

    // test valueAt
    XCTAssertTrue(sampler.valueAt(vec3(radius / 2, 0, 0), fuzz) == 1, @"Point inside sphere is inside");
    XCTAssertTrue(sampler.valueAt(vec3(radius * 2, 0, 0), fuzz) == 0, @"Point outside sphere is outside");
    XCTAssertTrue(sampler.valueAt(vec3(radius, 0, 0), fuzz) == 0, @"Point on boundary is outside");
    XCTAssertTrue(sampler.valueAt(vec3(radius - fuzz, 0, 0), fuzz) == 1, @"Point on inner fuzz boundary is inside");
    XCTAssertEqualWithAccuracy(sampler.valueAt(vec3(radius - fuzz / 2, 0, 0), fuzz),
        0.5F,
        EPSILON,
        @"Point halfway across fuzz boundary is half in/half out");

    // test AABB intersect
    XCTAssertTrue(sampler.test(AABB(vec3(0, 0, 0), 1)), @"unit AABB intersects sphere");
    XCTAssertTrue(sampler.test(AABB(vec3(radius, 0, 0), 1)), @"tangential unit AABB intersects sphere");
    XCTAssertFalse(sampler.test(AABB(vec3(2 * radius, 0, 0), 1)), @"unit AABB outside sphere");

    XCTAssertTrue(sampler.test(AABB(vec3(0, 0, 0), 2 * radius)), @"enclosing AABB intersects sphere");
    XCTAssertTrue(sampler.test(AABB(vec3(radius, 0, 0), 2 * radius)), @"tangential enclosing AABB intersects sphere");
    XCTAssertFalse(sampler.test(AABB(vec3(3 * radius, 0, 0), 2 * radius)), @"distant AABB doesn't intersect sphere");
}

- (void)testPlaneSampler
{

    // a simple plane on XZ facing up +Y
    // inside volume is from -0.5 to +0.5 on Y
    auto origin = vec3(0, 0, 0);
    auto normal = vec3(0, 1, 0);
    auto thickness = 1.0F;
    auto fuzz = 0.5F;
    auto sampler = BoundedPlaneVolumeSampler(origin, normal, thickness, IVolumeSampler::Mode::Additive);

    XCTAssertEqualWithAccuracy(sampler.valueAt(vec3(0, 0, 0), fuzz),
        1.0F,
        EPSILON,
        @"Point at origin of bounded plane is inside");

    XCTAssertEqualWithAccuracy(sampler.valueAt(vec3(0, 0.25F, 0), fuzz),
        0.5F,
        EPSILON,
        @"Point at halfway across fuzziness boundary is half-inside");

    XCTAssertEqualWithAccuracy(sampler.valueAt(vec3(0, -0.25F, 0), fuzz),
        0.5F,
        EPSILON,
        @"Point at halfway across fuzziness boundary is half-inside");

    XCTAssertEqualWithAccuracy(sampler.valueAt(vec3(0, 0.5F, 0), fuzz),
        0.0F,
        EPSILON,
        @"Point at fuzziness edge of bounded plane is outside");

    XCTAssertEqualWithAccuracy(sampler.valueAt(vec3(0, -0.5F, 0), fuzz),
        0.0F,
        EPSILON,
        @"Point at fuzziness edge of bounded plane is outside");

    XCTAssertEqualWithAccuracy(sampler.valueAt(vec3(0, 10.0F, 0), fuzz),
        0.0F,
        EPSILON,
        @"Point beyond fuzziness edge of bounded plane is outside");

    XCTAssertEqualWithAccuracy(sampler.valueAt(vec3(0, -10.0F, 0), fuzz),
        0.0F,
        EPSILON,
        @"Point beyond fuzziness edge of bounded plane is outside");

    // test AABB intersect
    XCTAssertTrue(sampler.test(AABB(vec3(0, 0, 0), 1)), @"unit AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(1, 0, 0), 1)), @"unit AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(-1, 0, 0), 1)), @"unit AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(0, 0, 1), 1)), @"unit AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(0, 0, -1), 1)), @"unit AABB intersects bounded plane");

    XCTAssertTrue(sampler.test(AABB(vec3(0, 0.5, 0), 1)), @"tangential AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(0, -0.5, 0), 1)), @"-tangential AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(1, 0.5, 0), 1)), @"tangential AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(-1, -0.5, 0), 1)), @"-tangential AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(0, 0.5, 1), 1)), @"tangential AABB intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(0, -0.5, -1), 1)), @"-tangential AABB intersects bounded plane");

    XCTAssertFalse(sampler.test(AABB(vec3(0, 1.5, 0), 1)), @"unit AABB at fuzziness boundary doesn't intersect bounded plane");
    XCTAssertFalse(sampler.test(AABB(vec3(0, -1.5, 0), 1)), @"unit AABB at -fuzziness boundary doesn't intersect bounded plane");

    XCTAssertTrue(sampler.test(AABB(vec3(0, 0, 0), 10)), @"Large straddling aabb intersects bounded plane");
    XCTAssertTrue(sampler.test(AABB(vec3(0, 4.75, 0), 10)), @"Large aabb with enclosed corners intersects bounded plane");
}

- (void)testOctreePartitioning
{
    // 16x16x16 - should get a max depth of 2 (16:0 -> 8:1 > 4:2) and 64 + 8 + 1 (8^0 + 8^1 + 8^2) nodes
    const int size = 16;
    const int minSize = 4;
    const int leafDepth = 2;
    auto octree = OctreeVolume { size, 1, minSize };

    int maxDepth = 0;
    int nodeCount = 0;
    octree.walkOctree([self, &maxDepth, &nodeCount, leafDepth, size](auto node) {
        maxDepth = max(maxDepth, node->depth);
        nodeCount++;

        if (node->isLeaf) {
            XCTAssertTrue(node->depth == leafDepth, @"Expect leaf node depth == 2");
        } else {
            XCTAssertTrue(node->depth < leafDepth, @"Expect !leaf node depth < 2");
        }

        auto bounds = node->bounds;
        int expectedSize = size >> node->depth;
        XCTAssertEqual(bounds.size().x, expectedSize);
        XCTAssertTrue(bounds.size().x == bounds.size().y);
        XCTAssertTrue(bounds.size().x == bounds.size().z);
    });

    XCTAssertEqual(maxDepth, 2, @"Expect octree node max depth == 2");
    XCTAssertEqual(nodeCount, 73, @"Expect node count to be 64");
}

- (void)testEmptyOctreeVolumeCulling
{
    // 16x16x16 - should get a max depth of 2 (16:0 -> 8:1 > 4:2) and 64 + 8 + 1 (8^0 + 8^1 + 8^2) nodes
    const int size = 16;
    const int minSize = 4;
    auto octree = OctreeVolume { size, 1, minSize };

    int called = 0;
    octree.visitOccupiedNodes(true, [&called](auto node) {
        called++;
    });
    XCTAssertEqual(called, 0, "Expect empty octree not to result in any march callback invocations");
}

- (void)testSimpleOctreeVolumeCulling
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
    XCTAssertEqual(result.size(), 1);
    XCTAssertTrue(result[0]->isLeaf);

    // small sphere at x = 4 and z = 4, but y = 0 should collect 4 leaf nodes
    sphere->setPosition(vec3(minSize, 0, minSize));
    result = collect();
    XCTAssertEqual(result.size(), 4);
    for (auto node : result) {
        XCTAssertTrue(node->isLeaf);
        XCTAssertEqual(node->depth, leafDepth);
    }

    // small sphere at x = 4 and z = 4, and y = 4 should collect 1 node at depth of 1
    // because it's in all 8 leaf nodes
    sphere->setPosition(vec3(minSize, minSize, minSize));
    result = collect();
    XCTAssertEqual(result.size(), 1);
    XCTAssertEqual(result[0]->depth, leafDepth - 1);
}

@end
