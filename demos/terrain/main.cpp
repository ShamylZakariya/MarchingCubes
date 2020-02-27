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
#include <mc/util/util.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

#include "../common/cubemap_blur.hpp"
#include "materials.hpp"
#include "terrain_samplers.hpp"
#include "xorshift.hpp"
#include "FastNoise.h"

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
constexpr float FOV_DEGREES = 80.0F;
constexpr float OCTREE_NODE_VISUAL_INSET_FACTOR = 0.0F;

struct VolumeSegment {
public:
    explicit VolumeSegment(int numThreadsToUse)
    {
        std::vector<unowned_ptr<mc::TriangleConsumer<mc::Vertex>>> unownedTriangleConsumers;
        for (auto i = 0; i < numThreadsToUse; i++) {
            triangles.push_back(make_unique<mc::TriangleConsumer<mc::Vertex>>());
            unownedTriangleConsumers.push_back(triangles.back().get());
        }

        volume = make_unique<mc::OctreeVolume>(256, 4, 4, unownedTriangleConsumers);
    }

    ~VolumeSegment() = default;
    VolumeSegment(const VolumeSegment&) = delete;
    VolumeSegment(VolumeSegment&&) = delete;
    VolumeSegment& operator==(const VolumeSegment&) = delete;

    int march()
    {
        occupiedAABBs.clear();

        volume->march([this](mc::OctreeVolume::Node* node) {
            {
                // update the occupied aabb display
                auto bounds = node->bounds;
                bounds.inset(node->depth * OCTREE_NODE_VISUAL_INSET_FACTOR);
                occupiedAABBs.add(bounds, nodeColor(node->depth));
            }
        });

        int triangleCount = 0;
        for (const auto& tc : triangles) {
            triangleCount += tc->numTriangles();
        }

        return triangleCount;
    }

    unique_ptr<mc::OctreeVolume> volume;
    unowned_ptr<GroundSampler> groundSampler;
    std::vector<unique_ptr<mc::TriangleConsumer<mc::Vertex>>> triangles;
    mc::util::LineSegmentBuffer occupiedAABBs;
    mat4 model;

private:
    std::vector<vec4> _nodeColors;

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
    mc::util::FpsCalculator _fpsCalculator;
    bool _running = true;
    int _triangleCount = 0;

    // render state
    std::unique_ptr<TerrainMaterial> _terrainMaterial;
    std::unique_ptr<LineMaterial> _lineMaterial;
    std::unique_ptr<SkydomeMaterial> _skydomeMaterial;
    std::vector<std::unique_ptr<VolumeSegment>> _segments;
    mc::TriangleConsumer<mc::util::VertexP3C4> _skydomeQuad;
    float _aspect = 1;
    bool _drawOctreeAABBs = false;

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    std::set<int> _pressedkeyScancodes;
    vec2 _lastMousePosition { -1 };

    struct _CameraState {
        float pitch = 0;
        float yaw = 0;
        vec3 position = vec3 { 0, 0, -100 };

        mat4 view() const
        {
            vec3 dir = vec3 { 0, 0, 1 };
            vec3 up = vec3 { 0, 1, 0 };
            mat3 rot = rotation();

            dir = rot * dir;
            up = rot * up;
            return lookAt(position, position + dir, up);
        }

        mat3 rotation() const
        {
            return rotate(rotate(mat4 { 1 }, yaw, vec3 { 0, 1, 0 }), pitch, vec3 { 1, 0, 0 });
        }

        void moveBy(vec3 deltaLocal)
        {
            vec3 deltaWorld = rotation() * deltaLocal;
            position += deltaWorld;
        }

    } _cameraState;

    // demo state (noise, etc)
    FastNoise _fastNoise;

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
        _fpsCalculator.reset();
        double lastTime = glfwGetTime();
        while (_running && !glfwWindowShouldClose(_window)) {
            glfwPollEvents();

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            double now = glfwGetTime();
            double elapsed = now - lastTime;
            lastTime = now;
            step(static_cast<float>(now), static_cast<float>(elapsed));

            _fpsCalculator.update();
            drawFrame();
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

        auto nThreads = std::thread::hardware_concurrency();
        std::cout << "Using " << nThreads << " threads to march volumes" << std::endl;

        // TODO: Decide how many sections we will create
        constexpr auto COUNT = 3;

        for (int i = 0; i < COUNT; i++) {
            _segments.emplace_back(std::make_unique<VolumeSegment>(nThreads));
        }

        //
        //  Build terrain
        //

        for (size_t i = 0; i < _segments.size(); i++) {
            buildTerrainSection(i);
        }

        marchSegments();
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
            marchSegments();
        }
    }

    void onKeyRelease(int key, int scancode, int mods)
    {
    }

    void onMouseMove(const vec2& pos, const vec2& delta)
    {
        if (_mouseButtonState[0]) {
            // simple x/y trackball
            float trackballSpeed = 0.004 * M_PI;
            _cameraState.pitch += delta.y * trackballSpeed;
            _cameraState.yaw += -delta.x * trackballSpeed;
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
            _cameraState.pitch -= lookSpeed;
        }

        if (isKeyDown(GLFW_KEY_DOWN)) {
            _cameraState.pitch += lookSpeed;
        }

        if (isKeyDown(GLFW_KEY_LEFT)) {
            _cameraState.yaw += lookSpeed;
        }

        if (isKeyDown(GLFW_KEY_RIGHT)) {
            _cameraState.yaw -= lookSpeed;
        }
    }

    void drawFrame()
    {

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view = _cameraState.view();
        mat4 projection = glm::perspective(radians(FOV_DEGREES), _aspect, NEAR_PLANE, FAR_PLANE);

        // draw volumes
        glDepthMask(GL_TRUE);
        float segmentZ = 0;
        for (const auto& segment : _segments) {
            segment->model = translate(mat4 { 1 }, vec3(0, 0, segmentZ));
            _terrainMaterial->bind(segment->model, view, projection, _cameraState.position);
            for (auto& tc : segment->triangles) {
                tc->draw();
            }

            segmentZ += segment->volume->size().z;
        }

        // draw skydome (after volume, it's depth is 1 in clip space)
        glDepthMask(GL_FALSE);
        _skydomeMaterial->bind(projection, view);
        _skydomeQuad.draw();

        if (_drawOctreeAABBs) {
            // draw lines
            glDepthMask(GL_FALSE);
            for (const auto& segment : _segments) {
                _lineMaterial->bind(projection * view * segment->model);
                segment->occupiedAABBs.draw();
            }
        }
        glDepthMask(GL_TRUE);
    }

    void drawGui()
    {
        ImGui::Begin("Demo window");

        ImGui::LabelText("FPS", "%2.1f", static_cast<float>(_fpsCalculator.getFps()));
        ImGui::LabelText("triangles", "%d", _triangleCount);

        ImGui::Separator();
        ImGui::Checkbox("AABBs", &_drawOctreeAABBs);

        ImGui::End();
    }

    void buildTerrainSection(int which)
    {
        std::cout << "Building segment " << which << std::endl;

        const auto& segment = _segments[which];

        // remove existing samplers - it's OK, making new ones is cheap
        segment->volume->clear();

        //
        // build terrain sampler
        //

        const float maxHeight = 8.0F;
        const float floorThreshold = 1.25F;
        const auto period = _segments.front()->volume->size().z * 1.0F;

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
            vec4(0.7,0.7,0.7,1),
            0,
            1,
            1
        };

        const mc::MaterialState archMaterial {
            vec4(0.5,0.5,0.5,1),
            0.3,
            0,
            1
        };

        _fastNoise.SetNoiseType(FastNoise::Simplex);
        _fastNoise.SetFrequency(1);

        const auto zOffset = which * segment->volume->size().z;

        const auto groundSampler3D = [=](vec3 p) {
            float v = _fastNoise.GetSimplex(p.x / period, p.y / period, (p.z + zOffset) / period);
            return v * 0.5F + 0.5F;
        };

        const auto groundSampler2D = [=](vec2 p) {
            float v = _fastNoise.GetSimplex(p.x / period, (p.y + zOffset) / period);
            return v * 0.5F + 0.5F;
        };

        segment->groundSampler = segment->volume->add(
            std::make_unique<GroundSampler>(groundSampler3D, groundSampler2D, maxHeight, floorThreshold,
                floorTerrainMaterial, lowTerrainMaterial, highTerrainMaterial));

        //
        //  Build an RNG seeded for this segment
        //

        auto rng = rng_xorshift64 { static_cast<uint64_t>(12345 * (which + 1)) };

        //
        // build a broken arch
        //

        const auto size = vec3(segment->volume->size());
        const auto center = size / 2.0F;

        const int nArches = rng.nextInt(7);
        std::cout << nArches << " arches" << std::endl;
        for (int i = 0; i < nArches; i++) {
            Tube::Config arch;
            arch.axisOrigin = vec3 {
                center.x + rng.nextFloat(-size.x / 10, size.x / 10),
                0,
                center.z + rng.nextFloat(-size.z / 3, size.z / 3)
            };

            arch.innerRadiusAxisOffset = vec3(0, rng.nextFloat(8, 16), 0);

            arch.axisDir = normalize(vec3(rng.nextFloat(-0.6, 0.6), rng.nextFloat(-0.2, 0.2), 1));

            arch.axisPerp = normalize(vec3(rng.nextFloat(-0.2, 0.2), 1, 0));
            arch.length = rng.nextFloat(7, 11);
            arch.innerRadius = rng.nextFloat(35, 43);
            arch.outerRadius = rng.nextFloat(48, 55);
            arch.frontFaceNormal = arch.axisDir;
            arch.backFaceNormal = -arch.axisDir;
            arch.cutAngleRadians = radians(rng.nextFloat(16, 32));
            arch.material = archMaterial;

            segment->volume->add(std::make_unique<Tube>(arch));
        }
    }

    void marchSegments()
    {
        _triangleCount = 0;
        for (const auto& s : _segments) {
            auto start = glfwGetTime();
            _triangleCount += s->march();
            auto elapsed = glfwGetTime() - start;
            std::cout << "March took " << elapsed << " seconds" << std::endl;
        }
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
