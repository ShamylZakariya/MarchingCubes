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
#include "FastNoise.h"
#include "camera.hpp"
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

constexpr bool AUTOPILOT = true;
constexpr int WIDTH = AUTOPILOT ? 506 : 1440;
constexpr int HEIGHT = AUTOPILOT ? 900 : 700;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 1000.0f;
constexpr float FOV_DEGREES = 50.0F;

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
#ifdef __APPLE__
        constexpr double SCALE = 1.25;
#else
        constexpr double SCALE = 2;
#endif
        // start imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(SCALE);

        const auto fontFile = "./fonts/ConsolaMono.ttf";
        if (std::filesystem::exists(fontFile)) {
            ImGui::GetIO().Fonts->AddFontFromFileTTF(fontFile, 12 * SCALE);
        }

        // set imgui platform/renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(_window, true);
#ifdef __APPLE__
        ImGui_ImplOpenGL3_Init("#version 330 core");
#else
        ImGui_ImplOpenGL3_Init();
#endif

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

            _axisMarker.addAxis(mat4 { 1 }, 64);
        }

        //
        // build a volume
        //

        _segmentSizeZ = 256;
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
            _segments.emplace_back(std::make_unique<TerrainSegment>(_segmentSizeZ, _threadPool));
            _segments.back()->build(i, _fastNoise);
            _segments.back()->march(false);
        }

        const auto size = vec3(_segments.front()->volume->size());
        const auto center = size / 2.0F;

        _lastWaypoint = vec3 { center.x, center.y, 0 };
        _nextWaypoint = _segments.front()->waypoints.front();

        if (!AUTOPILOT) {
            vec3 pos = center + vec3(-2 * size.x, 0, 0);
            _camera.lookAt(pos, center);
        } else {
            updateCamera(0);
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
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view = _camera.view();
        mat4 projection = glm::perspective(radians(FOV_DEGREES), _aspect, NEAR_PLANE, FAR_PLANE);

        // draw volumes
        glDepthMask(GL_TRUE);

        for (size_t i = 0, N = _segments.size(); i < N; i++) {
            const auto& segment = _segments[i];
            const auto model = translate(mat4 { 1 }, vec3(0, 0, (i * _segmentSizeZ) - _distanceAlongZ));
            _terrainMaterial->bind(model, view, projection, _camera.position);
            for (auto& tc : segment->triangles) {
                tc->draw();
            }
        }

        // draw skydome (after volume, it's depth is 1 in clip space)
        glDepthMask(GL_FALSE);
        _skydomeMaterial->bind(projection, view);
        _skydomeQuad.draw();

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
        ImGui::Checkbox("Scrolling", &_scrolling);

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

            seg->build(_segments.back()->idx + 1, _fastNoise);
            seg->march(false);

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
                for (const auto& waypoint : segment->waypoints) {
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
                _camera.rotateBy(0, -lookSpeed);
            }

            if (isKeyDown(GLFW_KEY_DOWN)) {
                _camera.rotateBy(0, lookSpeed);
            }

            if (isKeyDown(GLFW_KEY_LEFT)) {
                _camera.rotateBy(-lookSpeed, 0);
            }

            if (isKeyDown(GLFW_KEY_RIGHT)) {
                _camera.rotateBy(+lookSpeed, 0);
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

private:
    // app state
    GLFWwindow* _window;
    double _elapsedFrameTime = 0;
    bool _running = true;

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    std::set<int> _pressedkeyScancodes;
    vec2 _lastMousePosition { -1 };

    // render state
    std::unique_ptr<TerrainMaterial> _terrainMaterial;
    std::unique_ptr<LineMaterial> _lineMaterial;
    std::unique_ptr<SkydomeMaterial> _skydomeMaterial;
    mc::TriangleConsumer<mc::util::VertexP3C4> _skydomeQuad;
    mc::util::LineSegmentBuffer _axisMarker;
    mc::util::LineSegmentBuffer _autopilotCameraAxis;
    float _aspect = 1;

    Camera _camera;

    // user input state
    bool _drawOctreeAABBs = false;
    bool _drawWaypoints = true;
    bool _drawSegmentBounds = true;
    bool _scrolling = false;

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
