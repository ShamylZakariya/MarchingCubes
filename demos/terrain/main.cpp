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

constexpr bool AUTOMATIC = false;
constexpr int WIDTH = AUTOMATIC ? 506 : 1440;
constexpr int HEIGHT = AUTOMATIC ? 900 : 700;
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

            _axisMarker.addAxis(mat4{1}, 64);
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

        // TODO: Decide how many sections we will use
        constexpr auto COUNT = 3;

        for (int i = 0; i < COUNT; i++) {
            _segments.emplace_back(std::make_unique<TerrainSegment>(_segmentSizeZ, nThreads));
            _segments.back()->build(i, _fastNoise);
            _segments.back()->march(false);
        }

        if (!AUTOMATIC) {
            vec3 size = vec3(_segments.front()->volume->size());
            vec3 center = size/2.0F;
            vec3 pos = center + vec3(-2*size.x, 0, 0);
            _camera.lookAt(pos, center);
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
        if (_mouseButtonState[0] && !AUTOMATIC) {
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
        mat4 terrainOffset = translate(mat4 { 1 }, vec3(0, 0, -_distanceAlongZ));

        for (const auto& segment : _segments) {
            _terrainMaterial->bind(segment->model * terrainOffset, view, projection, _camera.position);
            for (auto& tc : segment->triangles) {
                tc->draw();
            }
        }

        // draw skydome (after volume, it's depth is 1 in clip space)
        glDepthMask(GL_FALSE);
        _skydomeMaterial->bind(projection, view);
        _skydomeQuad.draw();

        // draw the origin of our scene
        _lineMaterial->bind(projection * view * mat4{1});
        _axisMarker.draw();

        // draw optional markers, aabbs, etc
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
        ImGui::Checkbox("Scrolling", &_scrolling);

        ImGui::End();
    }

    void updateScroll(float deltaT)
    {
        const float scrollSpeed = 100 * deltaT;
        if (_scrolling) {
            _distanceAlongZ += scrollSpeed;
        } else if (AUTOMATIC) {
            if (isKeyDown(GLFW_KEY_W)) {
                _distanceAlongZ += scrollSpeed;
            }
        }

        // TODO: check if _distanceAlongZ is *big* and reset all the scroll
        // to reduce float precision error, since this is meant to
        // run a long time uninterrupted. NOTE: This requires
        // changing TerrainSegment::model to not be an absolute distance!

        const int currentIdx = _distanceAlongZ / _segmentSizeZ;
        if (currentIdx < _segments.front()->idx) {
            // we moved backwards and revealed a new segment.
            // pop last segment, rebuild it for currentIdx and
            // push it front

            auto seg = std::move(_segments.back());
            _segments.pop_back();

            seg->build(currentIdx, _fastNoise);
            seg->march(false);

            _segments.push_front(std::move(seg));

        } else if (currentIdx > _segments.front()->idx) {
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
        if (AUTOMATIC) {

            // the environment is scrolling; the camera is always at z = 0
            // find the two waypoints the camera currently is inclusively between
            vec3 firstWaypointPosition = [=]() -> vec3 {
                for (const auto& seg : _segments) {
                    for (const auto& waypoint : seg->waypoints) {
                        return waypoint;
                    }
                }
                const auto& firstSeg = _segments.front();
                return vec3(firstSeg->volume->size()) / 2.0F;
            }();

            vec3 prevWaypoint(firstWaypointPosition.x, firstWaypointPosition.y, 0);
            vec3 nextWaypoint;
            bool found = false;
            int segIdx = 0;
            for (const auto& seg : _segments) {
                int wayPointIdx = 0;
                for (auto waypoint : seg->waypoints) {
                    waypoint = vec3(seg->model * vec4(waypoint, 1));
                    waypoint.z -= _distanceAlongZ;
                    if (waypoint.z <= 0) {
                        prevWaypoint = waypoint;
                    } else {
                        nextWaypoint = waypoint;
                        found = true;
                        break;
                    }
                    wayPointIdx++;
                }
                if (found) {
                    break;
                }
                segIdx++;
            }

            if (found) {
                float t = (_camera.position.z - prevWaypoint.z) / (nextWaypoint.z - prevWaypoint.z);
                vec3 position = mix(prevWaypoint, nextWaypoint, t);
                _camera.lookAt(position, nextWaypoint);
            }

        } else {
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
    float _aspect = 1;


    Camera _camera;

    // user input state
    bool _drawOctreeAABBs = false;
    bool _drawWaypoints = true;
    bool _drawSegmentBounds = true;
    bool _scrolling = false;

    // demo state
    float _distanceAlongZ = 0;
    int _segmentSizeZ = 0;
    std::deque<std::unique_ptr<TerrainSegment>> _segments;
    FastNoise _fastNoise;
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
