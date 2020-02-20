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
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <epoxy/gl.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <mc/marching_cubes.hpp>
#include <mc/util/util.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

#include "../common/cubemap_blur.hpp"
#include "materials.hpp"

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
    explicit VolumeSegment(int numThreadsToUse)
    {
        std::vector<unowned_ptr<mc::TriangleConsumer<mc::Vertex>>> unownedTriangleConsumers;
        for (auto i = 0; i < numThreadsToUse; i++) {
            triangles.push_back(make_unique<mc::TriangleConsumer<mc::Vertex>>());
            unownedTriangleConsumers.push_back(triangles.back().get());
        }

        volume = make_unique<mc::OctreeVolume>(128, 0.5F, 4, unownedTriangleConsumers);
    }

    ~VolumeSegment() = default;
    VolumeSegment(const VolumeSegment&) = delete;
    VolumeSegment(VolumeSegment&&) = delete;
    VolumeSegment& operator==(const VolumeSegment&) = delete;

    int march() {
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
    std::vector<unique_ptr<mc::TriangleConsumer<mc::Vertex>>> triangles;
    mc::util::LineSegmentBuffer occupiedAABBs;

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
    std::unique_ptr<VolumeMaterial> _volumeMaterial;
    std::unique_ptr<LineMaterial> _lineMaterial;
    std::unique_ptr<SkydomeMaterial> _skydomeMaterial;
    std::vector<std::unique_ptr<VolumeSegment>> _volumeSegments;
    mc::TriangleConsumer<mc::util::VertexP3C4> _skydomeQuad;
    float _aspect = 1;

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    vec2 _lastMousePosition { -1 };

    // camera state
    mat3 _cameraRotation { 1 };
    vec3 _cameraPosition { 0, 0, -10 };

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

        _window = glfwCreateWindow(WIDTH, HEIGHT, "Marching Cubes", nullptr, nullptr);
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
                app->onKeyPress(key, scancode, mods);
                break;
            case GLFW_RELEASE:
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

        float volumeShininess = 0.0f;
        vec3 ambientLight { 0.0f, 0.0f, 0.0f };

        auto skyboxTexture = mc::util::LoadTextureCube("textures/skybox", ".jpg");
        auto lightprobeTex = BlurCubemap(skyboxTexture, radians<float>(90), 8);

        _volumeMaterial = std::make_unique<VolumeMaterial>(std::move(lightprobeTex), ambientLight, skyboxTexture, volumeShininess);
        _lineMaterial = std::make_unique<LineMaterial>();
        _skydomeMaterial = std::make_unique<SkydomeMaterial>(skyboxTexture);

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

        for (int i = 0; i < 3; i++) {
            _volumeSegments.emplace_back(std::make_unique<VolumeSegment>(nThreads));
        }

        //
        //  Build terrain
        //

        for (int i = 0; i < _volumeSegments.size(); i++) {
            buildTerrain(i);
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
        if (scancode == glfwGetKeyScancode(GLFW_KEY_ESCAPE) || scancode == glfwGetKeyScancode(GLFW_KEY_Q)) {
            _running = false;
        }

        // WASD
        if (scancode == glfwGetKeyScancode(GLFW_KEY_A)) {
            moveCamera(vec3(-1,0,0));
        }

        if (scancode == glfwGetKeyScancode(GLFW_KEY_D)) {
            moveCamera(vec3(+1,0,0));
        }

        if (scancode == glfwGetKeyScancode(GLFW_KEY_W)) {
            moveCamera(vec3(0,0,-1));
        }

        if (scancode == glfwGetKeyScancode(GLFW_KEY_S)) {
            moveCamera(vec3(0,0,+1));
        }

        // if (scancode == glfwGetKeyScancode(GLFW_KEY_Q)) {
        //     moveCamera(vec3(0,-1,0));
        // }

        // if (scancode == glfwGetKeyScancode(GLFW_KEY_E)) {
        //     moveCamera(vec3(0,+1,0));
        // }
    }

    void onKeyRelease(int key, int scancode, int mods)
    {
    }

    void onMouseMove(const vec2& pos, const vec2& delta)
    {
        if (_mouseButtonState[0]) {
            // simple x/y trackball
            float trackballSpeed = 0.004 * M_PI;
            mat3 xRot = rotate(mat4(1), -1 * delta.y * trackballSpeed, vec3(1, 0, 0));
            mat3 yRot = rotate(mat4(1), 1 * delta.x * trackballSpeed, vec3(0, 1, 0));
            _cameraRotation = _cameraRotation * xRot * yRot;
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
    }

    void drawFrame()
    {

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view = inverse(translate(mat4{1}, _cameraPosition) * mat4{_cameraRotation});
        mat4 projection = glm::perspective(radians(FOV_DEGREES), _aspect, NEAR_PLANE, FAR_PLANE);

        // draw volumes
        glDepthMask(GL_TRUE);
        float segmentZ = 0;
        for (const auto& segment : _volumeSegments) {
            mat4 model = translate(mat4{1}, vec3(0,0,segmentZ));
            _volumeMaterial->bind(model, view, projection, _cameraPosition);
            for (auto& tc : segment->triangles) {
                tc->draw();
            }

            segmentZ += segment->volume->size().z;
        }

        // draw skydome (after volume, it's depth is 1 in clip space)
        glDepthMask(GL_FALSE);
        _skydomeMaterial->bind(projection, view);
        _skydomeQuad.draw();

        // draw lines
        glDepthMask(GL_FALSE);
        segmentZ = 0;
        for (const auto& segment : _volumeSegments) {
            // TODO: Store model in segment
            mat4 model = translate(mat4{1}, vec3(0,0,segmentZ));
            _lineMaterial->bind(projection * view * model);
            segment->occupiedAABBs.draw();
            segmentZ += segment->volume->size().z;
        }

        glDepthMask(GL_TRUE);
    }

    void drawGui()
    {
        ImGui::Begin("Demo window");

        ImGui::LabelText("FPS", "%2.1f", static_cast<float>(_fpsCalculator.getFps()));
        ImGui::LabelText("triangles", "%d", _triangleCount);

        ImGui::End();
    }

    void buildTerrain(int which)
    {
        std::cout << "Building segment " << which << std::endl;

        // for now make a box
        const auto &segment = _volumeSegments[which];
        const auto size = vec3{segment->volume->size()};
        const auto center = size / 2.0F;
        segment->volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
            vec3(center.x, 10, center.z),
            vec3(size.x/2, 19, size.y/2),
            mat3{1},
            mc::IVolumeSampler::Mode::Additive
        ));
    }

    void marchSegments()
    {
        _triangleCount = 0;
        for (const auto &s : _volumeSegments) {
            _triangleCount += s->march();
        }
    }

    void moveCamera(vec3 deltaInLook) {
        vec3 deltaWorld = _cameraRotation * deltaInLook;
        _cameraPosition += deltaWorld;
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
