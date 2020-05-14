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
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <epoxy/gl.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <mc/util/op_queue.hpp>
#include <mc/util/util.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

#include "../common/cubemap_blur.hpp"
#include "../common/post_processing_stack.hpp"
#include "FastNoise.h"
#include "camera.hpp"
#include "filters.hpp"
#include "spring.hpp"
#include "terrain_segment.hpp"

using namespace glm;
using mc::util::AABB;
using mc::util::iAABB;
using mc::util::unowned_ptr;
using std::make_unique;
using std::unique_ptr;

//
// Constants
//

constexpr bool AUTOPILOT = false;
constexpr int WIDTH = 1440;
constexpr int HEIGHT = 1100;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 1000.0f;
constexpr float FOV_DEGREES = 50.0F;
constexpr double UI_SCALE = 1.5;

constexpr int PIXEL_SCALE = 1;
constexpr bool PALETTIZE = false;

//
// App
//

class App {
public:
    App()
    {
        initWindow();
        initApp();
    }

    ~App() = default;

    void run()
    {
        _running = true;

        // start imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(UI_SCALE);

        const auto fontFile = "./fonts/ConsolaMono.ttf";
        if (std::filesystem::exists(fontFile)) {
            ImGui::GetIO().Fonts->AddFontFromFileTTF(fontFile, 12 * UI_SCALE);
        }

        // set imgui platform/renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(_window, true);
        ImGui_ImplOpenGL3_Init();

        // enter run loop
        double lastTime = glfwGetTime();
        while (isRunning()) {
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
    bool isRunning()
    {
        if (_running && !glfwWindowShouldClose(_window)) {
            return true;
        }

        // looks like we should terminate; but check first that
        // we don't have any active march jobs running
        for (const auto& seg : _segments) {
            if (seg->isWorking()) {
                return true;
            }
        }

        // looks like we can safely terminate
        return false;
    }

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
        _segmentSizeZ = 256;

        //
        // load materials
        //

        const auto ambientLight = vec3(0.45, 0.5, 0.0);
        const auto skyboxTexture = mc::util::LoadTextureCube("textures/sky", ".jpg");
        const mc::util::TextureHandleRef lightprobeTex = BlurCubemap(skyboxTexture, radians<float>(90), 8);
        const auto terrainTexture0 = mc::util::LoadTexture2D("textures/granite.jpg");
        const auto terrainTexture0Scale = 30;
        const auto terrainTexture1 = mc::util::LoadTexture2D("textures/cracked-asphalt.jpg");
        const auto terrainTexture1Scale = 30;
        const auto renderDistance = _segmentSizeZ * 2;

        _terrainMaterial = std::make_unique<TerrainMaterial>(
            ambientLight,
            lightprobeTex, skyboxTexture,
            terrainTexture0, terrainTexture0Scale,
            terrainTexture1, terrainTexture1Scale);

        _lineMaterial = std::make_unique<LineMaterial>();

        //
        // some constant GL state
        //

        glClearColor(0, 0, 0, 0);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
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

        _axisMarker.addAxis(mat4 { 1 }, 64);

        //
        //  Build a post-processing stack
        //

        _postProcessingFilters = std::make_unique<post_processing::FilterStack>();

        const auto noiseTexture = mc::util::LoadTexture2D("textures/noise.png");
        _atmosphere = _postProcessingFilters->push(std::make_unique<AtmosphereFilter>("Atmosphere", skyboxTexture, noiseTexture));
        _atmosphere->setRenderDistance(renderDistance * 0.5, renderDistance);
        _atmosphere->setAlpha(1);
        _atmosphere->setAtmosphericTint(vec4(1, 0.85, 1, 1));
        _atmosphere->setGroundFog(30, 0.00025, vec4(1, 0.63, 0.46, 1));

        if (PALETTIZE) {
            auto palettizer = _postProcessingFilters->push(std::make_unique<PalettizeFilter>(
                "Palettizer",
                ivec3(32, 32, 32),
                PalettizeFilter::ColorSpace::YUV));
            palettizer->setAlpha(1);
        }

        //
        // build a volume
        //

        const auto frequency = 1.0F / _segmentSizeZ;
        _fastNoise.SetNoiseType(FastNoise::Simplex);
        _fastNoise.SetFrequency(frequency);

        const auto nThreads = std::thread::hardware_concurrency();
        std::cout << "Using " << nThreads << " threads to march volumes" << std::endl;
        _threadPool = std::make_shared<mc::util::ThreadPool>(nThreads, true);

        //
        // build our terrain segments;
        // TODO: Decide how many segments we need
        //

        constexpr auto COUNT = 3;
        for (int i = 0; i < COUNT; i++) {
            _segments.emplace_back(std::make_unique<TerrainSegment>(_segmentSizeZ, _threadPool, _fastNoise));
            _segments.back()->build(i);
            _segments.back()->march();
        }

        const auto size = vec3(_segments.front()->getVolume()->size());
        const auto center = size / 2.0F;

        _lastWaypoint = vec3 { center.x, center.y, 0 };
        _nextWaypoint = _segments.front()->getWaypoints().front();

        if (AUTOPILOT) {
            updateCamera(0);
        } else {
            auto pos = vec3(center.x, size.y * 0.2F, 0);
            auto lookTarget = pos + vec3(0, 0, 1);
            _camera.lookAt(pos, lookTarget);
        }
    }

    void onResize(int width, int height)
    {
        _contextSize = ivec2 { width, height };
        glViewport(0, 0, width, height);
        _aspect = static_cast<float>(width) / static_cast<float>(height);
    }

    void onKeyPress(int key, int scancode, int mods)
    {
        if (scancode == glfwGetKeyScancode(GLFW_KEY_ESCAPE)) {
            _running = false;
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
        if (_mouseButtonState[0] && !AUTOPILOT) {
            // simple x/y trackball
            float trackballSpeed = 0.004F * pi<float>();
            float deltaPitch = -delta.y * trackballSpeed;
            float deltaYaw = +delta.x * trackballSpeed;
            _camera.rotateBy(deltaYaw, deltaPitch);
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
        mat4 view = _camera.view();
        mat4 projection = glm::perspective(radians(FOV_DEGREES), _aspect, NEAR_PLANE, FAR_PLANE);

        _atmosphere->setCameraState(_camera.position, projection, view, NEAR_PLANE, FAR_PLANE);

        _postProcessingFilters->execute(_contextSize / PIXEL_SCALE, _contextSize, [=]() {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // draw volumes
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);

            for (size_t i = 0, N = _segments.size(); i < N; i++) {
                const auto& segment = _segments[i];
                const auto model = translate(mat4 { 1 }, vec3(0, 0, (i * _segmentSizeZ) - _distanceAlongZ));
                _terrainMaterial->bind(model, view, projection, _camera.position);
                for (auto& buffer : segment->getGeometry()) {
                    buffer->draw();
                }
            }
        });

        glViewport(0, 0, _contextSize.x, _contextSize.y);

        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        // draw the origin of our scene
        _lineMaterial->bind(projection * view * mat4 { 1 });
        _axisMarker.draw();

        if (!AUTOPILOT) {
            _autopilotCameraAxis.draw();
        }

        // draw optional markers, aabbs, etc
        if (_drawOctreeAABBs || _drawWaypoints || _drawSegmentBounds) {
            for (size_t i = 0, N = _segments.size(); i < N; i++) {
                const auto& segment = _segments[i];
                const auto model = translate(mat4 { 1 }, vec3(0, 0, (i * _segmentSizeZ) - _distanceAlongZ));
                _lineMaterial->bind(projection * view * model);
                if (_drawSegmentBounds) {
                    segment->getBoundingLineBuffer().draw();
                }
                if (_drawOctreeAABBs) {
                    segment->getAabbLineBuffer().draw();
                }
                if (_drawWaypoints) {
                    segment->getWaypointLineBuffer().draw();
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

        double avgMarchDuration = 0;
        for (const auto& s : _segments) {
            avgMarchDuration += s->getLastMarchDurationSeconds();
        }
        avgMarchDuration /= _segments.size();
        ImGui::LabelText("march duration", "%.2fs", avgMarchDuration);

        ImGui::Separator();
        ImGui::Checkbox("Draw AABBs", &_drawOctreeAABBs);
        ImGui::Checkbox("Draw Waypoints", &_drawWaypoints);
        ImGui::Checkbox("Draw Segment Bounds", &_drawSegmentBounds);

        ImGui::End();
    }

    void updateScroll(float deltaT)
    {
        const float scrollDelta = _scrolling || isKeyDown(GLFW_KEY_RIGHT_BRACKET)
            ? 100 * deltaT
            : 0;

        _distanceAlongZ += scrollDelta;

        if (_distanceAlongZ > _segmentSizeZ) {
            _distanceAlongZ -= _segmentSizeZ;

            // we moved forward enough to hide first segment,
            // it can be repurposed. pop front segment, generate
            // a new end segment, and push it back

            auto seg = std::move(_segments.front());
            _segments.pop_front();

            seg->build(_segments.back()->getIdx() + 1);
            seg->march();

            _segments.push_back(std::move(seg));
        }

        _lastWaypoint.z -= scrollDelta;
        _nextWaypoint.z -= scrollDelta;

        if (_nextWaypoint.z <= 0) {
            _lastWaypoint = _nextWaypoint;

            // find the next waypoint
            for (size_t i = 0, N = _segments.size(); i < N; i++) {
                bool found = false;
                float dz = (i * _segmentSizeZ) - _distanceAlongZ;
                const auto& segment = _segments[i];
                for (const auto& waypoint : segment->getWaypoints()) {
                    auto waypointWorld = vec3(waypoint.x, waypoint.y, waypoint.z + dz);
                    if (waypointWorld.z > 0) {
                        _nextWaypoint = waypointWorld;
                        found = true;
                        break;
                    }
                }
                if (found) {
                    break;
                }
            }
        }
    }

    void updateCamera(float deltaT)
    {
        float t = (0 - _lastWaypoint.z) / (_nextWaypoint.z - _lastWaypoint.z);
        vec3 autoPilotPosition = mix(_lastWaypoint, _nextWaypoint, t);

        _autopilotPositionSpring.setTarget(autoPilotPosition);
        _autopilotTargetSpring.setTarget(_nextWaypoint);

        autoPilotPosition = _autopilotPositionSpring.step(deltaT);
        vec3 autoPilotTargetPosition = _autopilotTargetSpring.step(deltaT);

        if (AUTOPILOT) {

            _camera.lookAt(autoPilotPosition, autoPilotTargetPosition);

        } else {

            _autopilotCameraAxis.clear();
            _autopilotCameraAxis.addMarker(autoPilotPosition, 6, vec4(1, 1, 1, 1));
            _autopilotCameraAxis.addMarker(autoPilotTargetPosition, 6, vec4(1, 1, 0, 1));

            // WASD
            const float movementSpeed = 20 * deltaT * (isKeyDown(GLFW_KEY_LEFT_SHIFT) ? 5 : 1);
            const float lookSpeed = radians<float>(90) * deltaT;

            if (isKeyDown(GLFW_KEY_A)) {
                _camera.moveBy(movementSpeed * vec3(+1, 0, 0));
            }

            if (isKeyDown(GLFW_KEY_D)) {
                _camera.moveBy(movementSpeed * vec3(-1, 0, 0));
            }

            if (isKeyDown(GLFW_KEY_W)) {
                _camera.moveBy(movementSpeed * vec3(0, 0, +1));
            }

            if (isKeyDown(GLFW_KEY_S)) {
                _camera.moveBy(movementSpeed * vec3(0, 0, -1));
            }

            if (isKeyDown(GLFW_KEY_Q)) {
                _camera.moveBy(movementSpeed * vec3(0, -1, 0));
            }

            if (isKeyDown(GLFW_KEY_E)) {
                _camera.moveBy(movementSpeed * vec3(0, +1, 0));
            }

            if (isKeyDown(GLFW_KEY_UP)) {
                _camera.rotateBy(0, +lookSpeed);
            }

            if (isKeyDown(GLFW_KEY_DOWN)) {
                _camera.rotateBy(0, -lookSpeed);
            }

            if (isKeyDown(GLFW_KEY_LEFT)) {
                _camera.rotateBy(-lookSpeed, 0);
            }

            if (isKeyDown(GLFW_KEY_RIGHT)) {
                _camera.rotateBy(+lookSpeed, 0);
            }
        }
    }

    int triangleCount()
    {
        return std::accumulate(_segments.begin(), _segments.end(), 0, [](int acc, const auto& seg) {
            return acc + seg->getTriangleCount();
        });
    }

    bool isKeyDown(int scancode) const
    {
        return _pressedkeyScancodes.find(glfwGetKeyScancode(scancode)) != _pressedkeyScancodes.end();
    }

private:
    // app state
    GLFWwindow* _window;
    double _elapsedFrameTime = 0;
    bool _running = false;
    ivec2 _contextSize { WIDTH, HEIGHT };

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    std::set<int> _pressedkeyScancodes;
    vec2 _lastMousePosition { -1 };

    // render state
    std::unique_ptr<TerrainMaterial> _terrainMaterial;
    std::unique_ptr<LineMaterial> _lineMaterial;
    mc::util::LineSegmentBuffer _axisMarker;
    mc::util::LineSegmentBuffer _autopilotCameraAxis;
    float _aspect = 1;
    mc::util::unowned_ptr<AtmosphereFilter> _atmosphere;

    Camera _camera;
    std::unique_ptr<post_processing::FilterStack> _postProcessingFilters;

    // user input state
    bool _drawOctreeAABBs = false;
    bool _drawWaypoints = !AUTOPILOT;
    bool _drawSegmentBounds = true;
    bool _scrolling = AUTOPILOT;

    // demo state
    std::shared_ptr<mc::util::ThreadPool> _threadPool;
    float _distanceAlongZ = 0;
    int _segmentSizeZ = 0;
    std::deque<std::unique_ptr<TerrainSegment>> _segments;
    FastNoise _fastNoise;
    vec3 _lastWaypoint, _nextWaypoint;
    spring3 _autopilotPositionSpring { 5, 40, 20 };
    spring3 _autopilotTargetSpring { 5, 20, 10 };
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
