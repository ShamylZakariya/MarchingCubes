//
//  main.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <epoxy/gl.h>
#include <glm/gtc/noise.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <mc/marching_cubes.hpp>
#include <mc/util/op_queue.hpp>
#include <mc/util/util.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

#include "../common/cubemap_blur.hpp"
#include "FastNoise.h"
#include "materials.hpp"
#include "terrain_samplers.hpp"
#include "xorshift.hpp"

using namespace glm;
using mc::util::AABB;
using mc::util::iAABB;
using mc::util::unowned_ptr;
using std::make_unique;
using std::unique_ptr;

//
// Constants
//

constexpr int WIDTH = 1440;
constexpr int HEIGHT = 900;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 1000.0f;
constexpr float FOV_DEGREES = 50.0F;
constexpr float OCTREE_NODE_VISUAL_INSET_FACTOR = 0.0F;

struct VolumeSegment {
public:
    VolumeSegment(int size, int numThreadsToUse)
        : size(size)
    {
        std::cout << "VolumeSegment::ctor size: " << size << " numThreadsToUse: " << numThreadsToUse << std::endl;
        std::vector<unowned_ptr<mc::TriangleConsumer<mc::Vertex>>> unownedTriangleConsumers;
        for (auto i = 0; i < numThreadsToUse; i++) {
            triangles.push_back(make_unique<mc::TriangleConsumer<mc::Vertex>>());
            unownedTriangleConsumers.push_back(triangles.back().get());
        }

        volume = make_unique<mc::OctreeVolume>(size, 4, 4, unownedTriangleConsumers);
    }

    ~VolumeSegment() = default;
    VolumeSegment(const VolumeSegment&) = delete;
    VolumeSegment(VolumeSegment&&) = delete;
    VolumeSegment& operator==(const VolumeSegment&) = delete;

    void build(int idx, FastNoise& noise)
    {
        std::cout << "[VolumeSegment::build] idx " << idx << std::endl;

        this->idx = idx;
        volume->clear();
        triangleCount = 0;
        for (auto& tc : triangles) {
            tc->clear();
        }

        waypoints.clear();
        waypointLineBuffer.clear();

        const auto segmentColor = rainbow(static_cast<float>(idx % 5) / 5.0F);
        const float sizeZ = volume->size().z;
        model = translate(mat4 { 1 }, vec3(0, 0, sizeZ * idx));

        //
        // build terrain sampler
        //

        const mc::MaterialState floorTerrainMaterial {
            vec4(1),
            1,
            0,
            0
        };

        const mc::MaterialState lowTerrainMaterial {
            vec4(1),
            0,
            1,
            0
        };

        const mc::MaterialState highTerrainMaterial {
            vec4(0.7, 0.7, 0.7, 1),
            0,
            1,
            1
        };

        const mc::MaterialState archMaterial {
            vec4(0.5, 0.5, 0.5, 1),
            0.3,
            0,
            1
        };

        const float maxHeight = 8.0F;
        const float floorThreshold = 1.25F;
        const auto zOffset = idx * sizeZ;
        const auto groundSampler = [=](vec3 p) {
            float v = noise.GetSimplex(p.x, p.y, p.z + zOffset);
            return v * 0.5F + 0.5F;
        };

        volume->add(std::make_unique<GroundSampler>(groundSampler, maxHeight, floorThreshold,
            floorTerrainMaterial, lowTerrainMaterial, highTerrainMaterial));

        //
        //  Build an RNG seeded for this segment
        //

        auto rng = rng_xorshift64 { static_cast<uint64_t>(12345 * (idx + 1)) };

        //
        // build a broken arch
        //

        const auto size = vec3(volume->size());
        const auto center = size / 2.0F;
        const auto maxArches = 7;
        for (int i = 0; i < maxArches; i++) {

            // roll the dice to see if we get an arch here
            if (rng.nextInt(10) < 5) {
                continue;
            }

            // we have an arch, get its z position, use that to feed simplex
            // noise to perturb x position smoothly. Note - we inset a bit to
            // reduce likelyhood of clipping
            // TODO: Use simplex to seed our arches too... this would prevent clipping
            // because arches would clone across segment boundaries
            float archZ = 30 + (sizeZ - 60) * static_cast<float>(i) / maxArches;
            float archX = center.x + noise.GetSimplex(archZ + zOffset, 0) * size.x * 0.125F;

            Tube::Config arch;
            arch.axisOrigin = vec3 { archX, 0, archZ };

            arch.innerRadiusAxisOffset = vec3(0, rng.nextFloat(4, 10), 0);

            arch.axisDir = normalize(vec3(rng.nextFloat(-0.6, 0.6), rng.nextFloat(-0.2, 0.2), 1));

            arch.axisPerp = normalize(vec3(rng.nextFloat(-0.2, 0.2), 1, 0));
            arch.length = rng.nextFloat(7, 11);
            arch.innerRadius = rng.nextFloat(35, 43);
            arch.outerRadius = rng.nextFloat(48, 55);
            arch.frontFaceNormal = arch.axisDir;
            arch.backFaceNormal = -arch.axisDir;
            arch.cutAngleRadians = radians(rng.nextFloat(16, 32));
            arch.material = archMaterial;

            volume->add(std::make_unique<Tube>(arch));

            vec3 waypoint = arch.axisOrigin + arch.axisPerp * arch.innerRadius * 0.4F;
            waypoints.push_back(waypoint);
            waypointLineBuffer.addMarker(waypoint, 4, segmentColor);
        }

        //
        //  Build a debug frame to show our volume
        //

        boundingLineBuffer.clear();
        boundingLineBuffer.add(AABB(vec3 { 0.0F }, size).inset(1), segmentColor);
    }

    void march(bool synchronously)
    {
        aabbLineBuffer.clear();

        const auto nodeObserver = [this](mc::OctreeVolume::Node* node) {
            {
                // update the occupied aabb display
                auto bounds = node->bounds;
                bounds.inset(node->depth * OCTREE_NODE_VISUAL_INSET_FACTOR);
                aabbLineBuffer.add(bounds, nodeColor(node->depth));
            }
        };

        const auto onMarchComplete = [this]() {
            triangleCount = 0;
            for (const auto& tc : triangles) {
                triangleCount += tc->numTriangles();
            }
        };

        if (synchronously) {
            volume->march(nodeObserver);
            onMarchComplete();
        } else {
            volume->marchAsync(onMarchComplete, nodeObserver);
        }
    }

    int idx = 0;
    int size = 0;
    int triangleCount;
    unique_ptr<mc::OctreeVolume> volume;
    std::vector<unique_ptr<mc::TriangleConsumer<mc::Vertex>>> triangles;
    mc::util::LineSegmentBuffer aabbLineBuffer;
    mc::util::LineSegmentBuffer boundingLineBuffer;
    mc::util::LineSegmentBuffer waypointLineBuffer;
    std::vector<vec3> waypoints;
    mat4 model;

private:
    std::vector<vec4> _nodeColors;

    vec4 rainbow(float dist) const
    {
        using namespace mc::util::color;
        const hsv hC { 360 * dist, 0.6F, 1.0F };
        const auto rgbC = Hsv2Rgb(hC);
        return vec4(rgbC.r, rgbC.g, rgbC.b, 1);
    }

    vec4 nodeColor(int atDepth)
    {
        using namespace mc::util::color;

        auto depth = volume->depth();
        if (_nodeColors.size() < depth) {
            _nodeColors.clear();
            const float hueStep = 360.0F / depth;
            for (auto i = 0U; i <= depth; i++) {
                const hsv hC { i * hueStep, 0.6F, 1.0F };
                const auto rgbC = Hsv2Rgb(hC);
                _nodeColors.emplace_back(rgbC.r, rgbC.g, rgbC.b, mix<float>(0.6, 0.25, static_cast<float>(i) / depth));
            }
        }

        return _nodeColors[atDepth];
    }
};

//
// App
//

class App {
private:
    // app state
    GLFWwindow* _window;
    double _elapsedFrameTime = 0;
    bool _running = true;

    // render state
    std::unique_ptr<TerrainMaterial> _terrainMaterial;
    std::unique_ptr<LineMaterial> _lineMaterial;
    std::unique_ptr<SkydomeMaterial> _skydomeMaterial;
    mc::TriangleConsumer<mc::util::VertexP3C4> _skydomeQuad;
    float _aspect = 1;
    bool _drawOctreeAABBs = false;
    bool _drawWaypoints = true;
    bool _drawSegmentBounds = true;
    bool _automaticCamera = false;
    bool _scrolling = false;

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    std::set<int> _pressedkeyScancodes;
    vec2 _lastMousePosition { -1 };

    struct _CameraState {
        mat3 look = mat3 { 1 };
        vec3 position = vec3 { 0, 0, -100 };

        mat4 view() const
        {
            auto up = glm::vec3 { look[0][1], look[1][1], look[2][1] };
            auto forward = glm::vec3 { look[0][2], look[1][2], look[2][2] };
            return lookAt(position, position + forward, up);
        }

        void moveBy(vec3 deltaLocal)
        {
            vec3 deltaWorld = inverse(look) * deltaLocal;
            position += deltaWorld;
        }

        void rotateBy(float yaw, float pitch)
        {
            auto right = glm::vec3 { look[0][0], look[1][0], look[2][0] };
            look = mat4 { look } * rotate(rotate(mat4 { 1 }, yaw, vec3 { 0, 1, 0 }), pitch, right);
        }

    } _cameraState;

    // demo state (noise, etc)
    std::deque<std::unique_ptr<VolumeSegment>> _segments;
    FastNoise _fastNoise;
    float _distanceAlongZ = 0;
    int _segmentSizeZ = 0;

public:
    App()
    {
        initWindow();
        initApp();
    }

    ~App() = default;

    void run()
    {
        // start imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(2);

        const auto fontFile = "./fonts/ConsolaMono.ttf";
        if (std::filesystem::exists(fontFile)) {
            ImGui::GetIO().Fonts->AddFontFromFileTTF(fontFile, 24);
        }

        // set imgui platform/renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(_window, true);
        ImGui_ImplOpenGL3_Init();

        // enter run loop
        double lastTime = glfwGetTime();
        while (_running && !glfwWindowShouldClose(_window)) {
            glfwPollEvents();

            // execute any queued operations
            mc::util::MainThreadQueue()->drain();

            // compute time delta and step simulation
            double now = glfwGetTime();
            double elapsed = now - lastTime;
            lastTime = now;
            step(static_cast<float>(now), static_cast<float>(elapsed));
            _elapsedFrameTime = (_elapsedFrameTime + elapsed) / 2;

            // draw scene
            drawFrame();

            // draw imgui ui
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            drawGui();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(_window);
        }

        // shutdown imgui
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

private:
    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        _window = glfwCreateWindow(WIDTH, HEIGHT, "Terrain", nullptr, nullptr);
        glfwSetWindowUserPointer(_window, this);
        glfwSetFramebufferSizeCallback(_window, [](GLFWwindow* window, int width, int height) {
            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            app->onResize(width, height);
        });

        glfwSetKeyCallback(_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            if (ImGui::GetIO().WantCaptureKeyboard) {
                return;
            }

            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            switch (action) {
            case GLFW_PRESS:
                app->_pressedkeyScancodes.insert(scancode);
                app->onKeyPress(key, scancode, mods);
                break;
            case GLFW_RELEASE:
                app->_pressedkeyScancodes.erase(scancode);
                app->onKeyRelease(key, scancode, mods);
                break;
            }
        });

        glfwSetMouseButtonCallback(_window, [](GLFWwindow* window, int button, int action, int mods) {
            if (ImGui::GetIO().WantCaptureMouse) {
                return;
            }

            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            switch (action) {
            case GLFW_PRESS:
                app->onMouseDown(button, mods);
                break;
            case GLFW_RELEASE:
                app->onMouseUp(button, mods);
                break;
            }
        });

        glfwSetScrollCallback(_window, [](GLFWwindow* window, double xOffset, double yOffset) {
            if (ImGui::GetIO().WantCaptureMouse) {
                return;
            }

            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            app->onMouseWheel(vec2(xOffset, yOffset));
        });

        glfwSetCursorPosCallback(_window, [](GLFWwindow* window, double xPos, double yPos) {
            auto app = reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
            vec2 pos { xPos, yPos };
            if (app->_lastMousePosition != vec2 { -1 }) {
                vec2 delta = pos - app->_lastMousePosition;
                app->onMouseMove(pos, delta);
            } else {
                app->onMouseMove(pos, vec2 { 0 });
            }
            app->_lastMousePosition = pos;
        });

        glfwMakeContextCurrent(_window);

        std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "OpenGL version supported: " << glGetString(GL_VERSION) << std::endl;
    }

    void initApp()
    {
        //
        // load materials
        //

        vec3 ambientLight { 0.0f, 0.0f, 0.0f };
        auto skyboxTexture = mc::util::LoadTextureCube("textures/skybox", ".jpg");
        auto backgroundTex = BlurCubemap(skyboxTexture, radians<float>(30), 64);
        auto lightprobeTex = BlurCubemap(skyboxTexture, radians<float>(90), 8);
        auto terrainTexture0 = mc::util::LoadTexture2D("textures/Metal_Hammered_001_basecolor.jpg");
        auto terrainTexture1 = mc::util::LoadTexture2D("textures/Metal_006_Base_Color.png");

        _terrainMaterial = std::make_unique<TerrainMaterial>(
            std::move(lightprobeTex), ambientLight, skyboxTexture,
            terrainTexture0, 20, terrainTexture1, 3.5);

        _lineMaterial = std::make_unique<LineMaterial>();
        _skydomeMaterial = std::make_unique<SkydomeMaterial>(std::move(backgroundTex));

        //
        // some constant GL state
        //

        glClearColor(0, 0, 0, 0);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); // required for cubemap miplevels

        //
        // force a single call to onResize so we can set up viewport, aspect ratio, etc
        //

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(_window, &fbWidth, &fbHeight);
        onResize(fbWidth, fbHeight);

        //
        // Build static geometry
        //

        {
            using V = decltype(_skydomeQuad)::vertex_type;
            _skydomeQuad.start();
            _skydomeQuad.addTriangle(mc::Triangle {
                V { vec3(-1, -1, 1), vec4(1, 0, 0, 1) },
                V { vec3(+1, -1, 1), vec4(0, 1, 0, 1) },
                V { vec3(+1, +1, 1), vec4(0, 0, 1, 1) } });
            _skydomeQuad.addTriangle(mc::Triangle {
                V { vec3(-1, -1, 1), vec4(0, 1, 1, 1) },
                V { vec3(+1, +1, 1), vec4(1, 0, 1, 1) },
                V { vec3(-1, +1, 1), vec4(1, 1, 0, 1) } });
            _skydomeQuad.finish();
        }

        //
        // build a volume
        //

        _segmentSizeZ = 256;
        const auto frequency = 1.0F / _segmentSizeZ;
        _fastNoise.SetNoiseType(FastNoise::Simplex);
        _fastNoise.SetFrequency(frequency);

        auto nThreads = std::thread::hardware_concurrency();
        std::cout << "Using " << nThreads << " threads to march volumes" << std::endl;

        // TODO: Decide how many sections we will create
        constexpr auto COUNT = 3;

        for (int i = 0; i < COUNT; i++) {
            _segments.emplace_back(std::make_unique<VolumeSegment>(_segmentSizeZ, nThreads));
            _segments.back()->build(i, _fastNoise);
            _segments.back()->march(false);
        }
    }

    void onResize(int width, int height)
    {
        glViewport(0, 0, width, height);
        _aspect = static_cast<float>(width) / static_cast<float>(height);
    }

    void onKeyPress(int key, int scancode, int mods)
    {
        if (scancode == glfwGetKeyScancode(GLFW_KEY_ESCAPE)) {
            _running = false;
        }
        if (scancode == glfwGetKeyScancode(GLFW_KEY_M)) {
            marchAllSegmentsSynchronously();
        }
        if (scancode == glfwGetKeyScancode(GLFW_KEY_SPACE)) {
            _scrolling = !_scrolling;
        }
    }

    void onKeyRelease(int key, int scancode, int mods)
    {
    }

    void onMouseMove(const vec2& pos, const vec2& delta)
    {
        if (_mouseButtonState[0] && !_automaticCamera) {
            // simple x/y trackball
            float trackballSpeed = 0.004F * pi<float>();
            float deltaPitch = -delta.y * trackballSpeed;
            float deltaYaw = +delta.x * trackballSpeed;
            _cameraState.rotateBy(deltaYaw, deltaPitch);
        }
    }

    void onMouseDown(int button, int mods)
    {
        if (button < 3) {
            _mouseButtonState[button] = true;
        }
    }

    void onMouseUp(int button, int mods)
    {
        if (button < 3) {
            _mouseButtonState[button] = false;
        }
    }

    void onMouseWheel(const vec2& scrollOffset)
    {
    }

    void step(float now, float deltaT)
    {
        updateScroll(deltaT);
        updateCamera(deltaT);
    }

    void drawFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view = _cameraState.view();
        mat4 projection = glm::perspective(radians(FOV_DEGREES), _aspect, NEAR_PLANE, FAR_PLANE);

        // draw volumes
        glDepthMask(GL_TRUE);
        mat4 terrainOffset = translate(mat4 { 1 }, vec3(0, 0, -_distanceAlongZ));

        for (const auto& segment : _segments) {
            _terrainMaterial->bind(segment->model * terrainOffset, view, projection, _cameraState.position);
            for (auto& tc : segment->triangles) {
                tc->draw();
            }
        }

        // draw skydome (after volume, it's depth is 1 in clip space)
        glDepthMask(GL_FALSE);
        _skydomeMaterial->bind(projection, view);
        _skydomeQuad.draw();

        if (_drawOctreeAABBs || _drawWaypoints || _drawSegmentBounds) {
            for (const auto& segment : _segments) {
                _lineMaterial->bind(projection * view * segment->model * terrainOffset);
                if (_drawSegmentBounds) {
                    segment->boundingLineBuffer.draw();
                }
                if (_drawOctreeAABBs) {
                    segment->aabbLineBuffer.draw();
                }
                if (_drawWaypoints) {
                    segment->waypointLineBuffer.draw();
                }
            }
        }

        glDepthMask(GL_TRUE);
    }

    void drawGui()
    {
        ImGui::Begin("Demo window");

        float fps = _elapsedFrameTime > 0 ? static_cast<float>(1 / _elapsedFrameTime) : 0;
        ImGui::LabelText("FPS", "%03.0f", fps);
        ImGui::LabelText("triangles", "%d", triangleCount());

        ImGui::Separator();
        ImGui::Checkbox("AABBs", &_drawOctreeAABBs);
        ImGui::Checkbox("Waypoints", &_drawWaypoints);
        ImGui::Checkbox("Auto Camera", &_automaticCamera);
        ImGui::Checkbox("Scrolling", &_scrolling);

        ImGui::End();
    }

    void updateScroll(float deltaT)
    {
        const float travelSpeed = 100 * deltaT;
        if (_scrolling) {
            _distanceAlongZ += travelSpeed;
        }

        int currentIdx = _distanceAlongZ / _segmentSizeZ;
        if (currentIdx != _segments.front()->idx) {
            std::cout << "currentIdx: " << currentIdx << " prevIdx: " << _segments.front()->idx << std::endl;
        }

        if (currentIdx < _segments.front()->idx) {
            std::cout << "BACKWARDS - currentIdx: " << currentIdx << " PrevIdx: " << _segments.front()->idx << std::endl;

            // we moved backwards and revealed a new segment.
            // pop last segment, rebuild it for currentIdx and
            // push it front

            auto seg = std::move(_segments.back());
            _segments.pop_back();

            seg->build(currentIdx, _fastNoise);
            seg->march(false);

            _segments.push_front(std::move(seg));

        } else if (currentIdx > _segments.front()->idx) {
            std::cout << "FORWARDS - currentIdx: " << currentIdx << " prevIdx: " << _segments.front()->idx << std::endl;

            // we moved forward enough to hide first segment,
            // it can be repurposed. pop front segment, generate
            // a new end segment, and push it back

            auto seg = std::move(_segments.front());
            _segments.pop_front();

            seg->build(_segments.back()->idx + 1, _fastNoise);
            seg->march(false);

            _segments.push_back(std::move(seg));
        }
    }

    void updateCamera(float deltaT)
    {
        if (_automaticCamera) {

        } else {
            // WASD
            const float movementSpeed = 20 * deltaT * (isKeyDown(GLFW_KEY_LEFT_SHIFT) ? 5 : 1);
            const float lookSpeed = radians<float>(90) * deltaT;

            if (isKeyDown(GLFW_KEY_A)) {
                _cameraState.moveBy(movementSpeed * vec3(+1, 0, 0));
            }

            if (isKeyDown(GLFW_KEY_D)) {
                _cameraState.moveBy(movementSpeed * vec3(-1, 0, 0));
            }

            if (isKeyDown(GLFW_KEY_W)) {
                _cameraState.moveBy(movementSpeed * vec3(0, 0, +1));
            }

            if (isKeyDown(GLFW_KEY_S)) {
                _cameraState.moveBy(movementSpeed * vec3(0, 0, -1));
            }

            if (isKeyDown(GLFW_KEY_Q)) {
                _cameraState.moveBy(movementSpeed * vec3(0, -1, 0));
            }

            if (isKeyDown(GLFW_KEY_E)) {
                _cameraState.moveBy(movementSpeed * vec3(0, +1, 0));
            }

            if (isKeyDown(GLFW_KEY_UP)) {
                _cameraState.rotateBy(0, -lookSpeed);
            }

            if (isKeyDown(GLFW_KEY_DOWN)) {
                _cameraState.rotateBy(0, lookSpeed);
            }

            if (isKeyDown(GLFW_KEY_LEFT)) {
                _cameraState.rotateBy(-lookSpeed, 0);
            }

            if (isKeyDown(GLFW_KEY_RIGHT)) {
                _cameraState.rotateBy(+lookSpeed, 0);
            }
        }
    }

    // this is mainly for performance testiung
    void marchAllSegmentsSynchronously()
    {
        std::cout << "[App::marchAllSegmentsSynchronously] ";
        for (const auto& s : _segments) {
            auto start = glfwGetTime();
            s->march(true);
            auto elapsed = glfwGetTime() - start;
            std::cout << "(" << s->idx << " @ " << elapsed << "s)";
        }
        std::cout << std::endl;
    }

    int triangleCount()
    {
        return std::accumulate(_segments.begin(), _segments.end(), 0, [](int acc, const auto& seg) {
            return acc + seg->triangleCount;
        });
    }

    bool isKeyDown(int scancode) const
    {
        return _pressedkeyScancodes.find(glfwGetKeyScancode(scancode)) != _pressedkeyScancodes.end();
    }
};

//
// Main
//

int main()
{
    try {
        App app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
