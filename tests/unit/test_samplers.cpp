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

#include <mc/util/util.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

using std::make_unique;
using namespace glm;
using namespace Catch::literals;
using namespace mc;
using namespace mc::util;

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

        //unit AABB slightly beyond fuzziness boundary doesn't intersect bounded plane
        REQUIRE_FALSE(sampler.intersects(AABB(vec3(0, 1.51, 0), 1)));

        //unit AABB at slightly beyond -fuzziness boundary doesn't intersect bounded plane
        REQUIRE_FALSE(sampler.intersects(AABB(vec3(0, -1.51, 0), 1)));

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
        cube.setHalfExtents(vec3(2, 2, 2));
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
            cube.setHalfExtents(vec3(2, 2, 2));
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