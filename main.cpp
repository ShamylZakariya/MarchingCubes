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
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <epoxy/gl.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <mc/marching_cubes.hpp>
#include <mc/triangle_soup.hpp>
#include <mc/util/util.hpp>
#include <mc/volume.hpp>
#include <mc/volume_samplers.hpp>

using namespace glm;
using mc::util::AABB;
using mc::util::iAABB;
using mc::util::unowned_ptr;

//
// Constants
//

constexpr int WIDTH = 1440;
constexpr int HEIGHT = 900;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 1000.0f;
constexpr float FOV_DEGREES = 50.0F;
constexpr float OCTREE_NODE_VISUAL_INSET_FACTOR = 0.0F;

//
// Data
//

struct ProgramState {
    GLuint program = 0;
    GLint uniformLocMVP = -1;
    GLint uniformLocModel = -1;

    ProgramState() = default;
    ProgramState(const ProgramState& other) = delete;
    ProgramState(const ProgramState&& other) = delete;

    ~ProgramState()
    {
        if (program > 0) {
            glDeleteProgram(program);
        }
    }

    void build(const std::string& vertPath, const std::string& fragPath)
    {
        using namespace mc::util;
        auto vertSrc = ReadFile(vertPath);
        auto fragSrc = ReadFile(fragPath);
        program = CreateProgram(vertSrc.c_str(), fragSrc.c_str());
        uniformLocMVP = glGetUniformLocation(program, "uMVP");
        uniformLocModel = glGetUniformLocation(program, "uModel");
    }

    void bind(const mat4& mvp, const mat4& model)
    {
        glUseProgram(program);
        glUniformMatrix4fv(uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
        glUniformMatrix4fv(uniformLocModel, 1, GL_FALSE, value_ptr(model));
    }
};

//
//  Geometry Demos
//

class Demo {
public:
    Demo() = default;
    virtual ~Demo() = default;

    virtual void build(unowned_ptr<mc::BaseCompositeVolume> volume) = 0;
    virtual void step(float time) = 0;
    virtual void drawDebugLines(mc::util::LineSegmentBuffer&) {}
};

class CubeDemo : public Demo {
public:
    CubeDemo() = default;

    void build(unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        auto center = size / 2.0F;

        _rect = volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
            center, vec3(10, 10, 10), mat3 { 1 }, mc::IVolumeSampler::Mode::Additive));
    }

    void step(float time) override
    {
        float angle = static_cast<float>(M_PI * time * 0.1);
        _rect->setRotation(rotate(mat4 { 1 }, angle, vec3(0, 0, 1)));
    }

    void drawDebugLines(mc::util::LineSegmentBuffer& debugLinebuf) override
    {
        debugLinebuf.add(_rect->bounds(), vec4(1, 1, 0, 1));
        _rect->addTo(debugLinebuf, vec4(0, 1, 1, 1));
    }

private:
    unowned_ptr<mc::RectangularPrismVolumeSampler> _rect;
};

class CutCubeDemo : public Demo {
public:
    CutCubeDemo() = default;

    void build(unowned_ptr<mc::BaseCompositeVolume> volume) override
    {
        auto size = vec3(volume->size());
        auto center = size / 2.0F;
        auto cubeSize = min(size.x, min(size.y, size.z)) * 0.25F;
        auto planeThickness = cubeSize * 0.2F;

        _rect = volume->add(std::make_unique<mc::RectangularPrismVolumeSampler>(
            center, vec3(cubeSize), mat3 { 1 }, mc::IVolumeSampler::Mode::Additive));

        _cuttingPlane = volume->add(std::make_unique<mc::BoundedPlaneVolumeSampler>(
            center, vec3(0, 1, 0), planeThickness, mc::IVolumeSampler::Mode::Subtractive));
    }

    void step(float time) override
    {
        float angle = static_cast<float>(M_PI * time * 0.1);
        _rect->setRotation(rotate(mat4 { 1 }, angle, normalize(vec3(0, 1, 1))));
    }

    void drawDebugLines(mc::util::LineSegmentBuffer& debugLinebuf) override
    {
        debugLinebuf.add(_rect->bounds(), vec4(1, 1, 0, 1));
        _rect->addTo(debugLinebuf, vec4(0, 1, 1, 1));
    }

private:
    unowned_ptr<mc::RectangularPrismVolumeSampler> _rect;
    unowned_ptr<mc::BoundedPlaneVolumeSampler> _cuttingPlane;
};

constexpr const char* Demos[] = { "Cube", "CutCube" };

std::unique_ptr<Demo> CreateDemo(const std::string& demoName, unowned_ptr<mc::BaseCompositeVolume> volume)
{
    std::unique_ptr<Demo> demo;
    if (demoName == "Cube") {
        demo = std::make_unique<CubeDemo>();
    } else if (demoName == "CutCube") {
        demo = std::make_unique<CutCubeDemo>();
    }

    if (demo) {
        demo->build(volume);
    }

    return demo;
}

//
// App
//

class App {
public:
    App()
    {
        initWindow();
        initGl();
        buildVolume();
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
    GLFWwindow* _window;
    mc::util::FpsCalculator _fpsCalculator;

    // render state
    ProgramState _volumeProgram, _lineProgram;
    std::vector<std::unique_ptr<mc::ITriangleConsumer>> _triangleConsumers;
    mc::util::LineSegmentBuffer _octreeAABBLineSegmentStorage;
    mc::util::LineSegmentBuffer _octreeOccupiedAABBsLineSegmentStorage;
    mc::util::LineSegmentBuffer _axes;
    mc::util::LineSegmentBuffer _debugLines;

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    vec2 _lastMousePosition { -1 };
    mat4 _model { 1 };
    mat3 _trackballRotation { 1 };

    // volume and samplers
    std::unique_ptr<mc::OctreeVolume> _volume;
    std::unique_ptr<Demo> _currentDemo;
    int _currentDemoIdx = 0;

    // app state
    enum class AABBDisplay : int {
        None,
        OctreeGraph,
        MarchNodes
    };

    bool _animateVolume = false;
    bool _running = true;
    bool _useOrthoProjection = false;
    AABBDisplay _aabbDisplay = AABBDisplay::None;
    bool _needsMarchVolume = true;
    float _fuzziness = 1.0F;
    float _aspect = 1;
    float _dolly = 1;
    float _animationTime = 0;
    bool _drawDebugLines = false;

    struct {
        int nodesMarched = 0;
        std::vector<int> nodesMarchedByDepth;
        int voxelsMarched = 0;
        int triangleCount = 0;

        void reset(int maxDepth)
        {
            nodesMarched = 0;
            nodesMarchedByDepth = std::vector<int>(maxDepth, 0);
            voxelsMarched = 0;
            triangleCount = 0;
        }
    } _marchStats;

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
    }

    void initGl()
    {
        std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "OpenGL version supported: " << glGetString(GL_VERSION) << std::endl;

        //
        // load programs
        //

        _volumeProgram.build("shaders/gl/volume_vert.glsl", "shaders/gl/volume_frag.glsl");
        _lineProgram.build("shaders/gl/line_vert.glsl", "shaders/gl/line_frag.glsl");

        //
        // some constant GL state
        //

        glClearColor(0, 0, 0, 0);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //
        // force update of the _proj matrix
        //

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(_window, &fbWidth, &fbHeight);
        onResize(fbWidth, fbHeight);

        _axes.clear();
        using mc::util::Vertex;
        _axes.add(Vertex { vec3(0, 0, 0), vec4(1, 0, 0, 1) }, Vertex { vec3(10, 0, 0), vec4(1, 0, 0, 1) });
        _axes.add(Vertex { vec3(0, 0, 0), vec4(0, 1, 0, 1) }, Vertex { vec3(0, 10, 0), vec4(0, 1, 0, 1) });
        _axes.add(Vertex { vec3(0, 0, 0), vec4(0, 0, 1, 1) }, Vertex { vec3(0, 0, 10), vec4(0, 0, 1, 1) });
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
        } else if (scancode == glfwGetKeyScancode(GLFW_KEY_SPACE)) {
            displayMarchStats();
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
            mat3 xRot = rotate(mat4(1), -1 * delta.y * trackballSpeed, vec3(1, 0, 0));
            mat3 yRot = rotate(mat4(1), 1 * delta.x * trackballSpeed, vec3(0, 1, 0));
            _trackballRotation = xRot * yRot * _trackballRotation;
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
        _dolly -= 0.025F * scrollOffset.y;
        _dolly = clamp<float>(_dolly, 0, 1);
    }

    void step(float now, float deltaT)
    {
        if (_animateVolume) {
            _animationTime += deltaT;
        }

        _currentDemo->step(_animationTime);

        if (_animateVolume || _needsMarchVolume) {
            marchVolume();
            _needsMarchVolume = false;
        }
    }

    void drawFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto mvp = [this]() {

            // extract trackball Z and Y for building view matrix via lookAt
            auto trackballY = glm::vec3 { _trackballRotation[0][1], _trackballRotation[1][1], _trackballRotation[2][1] };
            auto trackballZ = glm::vec3 { _trackballRotation[0][2], _trackballRotation[1][2], _trackballRotation[2][2] };
            mat4 view, proj;

            if (_useOrthoProjection) {
                auto bounds = _volume->bounds();
                auto size = length(bounds.size());

                auto scaleMin = 0.1F;
                auto scaleMax = 5.0F;
                auto scale = mix(scaleMin, scaleMax, pow<float>(_dolly, 2.5));

                auto width = scale * _aspect * size;
                auto height = scale * size;

                auto distance = FAR_PLANE / 2;
                view = lookAt(-distance * trackballZ, vec3(0), trackballY);

                proj = glm::ortho(-width / 2, width / 2, -height / 2, height / 2, NEAR_PLANE, FAR_PLANE);
            } else {
                auto bounds = _volume->bounds();
                auto minDistance = 0.1F;
                auto maxDistance = length(bounds.size()) * 2;

                auto distance = mix(minDistance, maxDistance, pow<float>(_dolly, 2));
                view = lookAt(-distance * trackballZ, vec3(0), trackballY);

                proj = glm::perspective(radians(FOV_DEGREES), _aspect, NEAR_PLANE, FAR_PLANE);
            }
            return proj * view * _model;
        }();

        _volumeProgram.bind(mvp, _model);

        glDepthMask(GL_TRUE);
        for (auto& tc : _triangleConsumers) {
            tc->draw();
        }

        glDepthMask(GL_FALSE);
        _lineProgram.bind(mvp, _model);

        _axes.draw();

        switch (_aabbDisplay) {
        case AABBDisplay::None:
            break;
        case AABBDisplay::OctreeGraph:
            _octreeAABBLineSegmentStorage.draw();
            break;
        case AABBDisplay::MarchNodes:
            _octreeOccupiedAABBsLineSegmentStorage.draw();
            break;
        }

        if (_drawDebugLines) {
            _debugLines.clear();
            _currentDemo->drawDebugLines(_debugLines);
            _debugLines.draw();
        }

        glDepthMask(GL_TRUE);
    }

    void drawGui()
    {
        ImGui::Begin("Demo window");

        ImGui::LabelText("FPS", "%2.1f", static_cast<float>(_fpsCalculator.getFps()));
        ImGui::LabelText("triangles", "%d", _marchStats.triangleCount);

        ImGui::Separator();

        if (ImGui::Combo("Demo", &_currentDemoIdx, Demos, IM_ARRAYSIZE(Demos))) {
            buildDemo(_currentDemoIdx);
        }

        ImGui::Separator();

        ImGui::Checkbox("Animate", &_animateVolume);
        if (!_animateVolume) {
            ImGui::SameLine();
            _needsMarchVolume = ImGui::Button("March Once");
        }

        ImGui::Separator();
        if (ImGui::SliderFloat("Fuzziness", &_fuzziness, 0, 5, "%.2f")) {
            _volume->setFuzziness(_fuzziness);
            _needsMarchVolume = true;
        }

        ImGui::Separator();

        ImGui::Text("Reset Trackball Rotation");
        if (ImGui::Button("+X")) {
            _trackballRotation = rotate(mat4 { 1 }, -half_pi<float>(), vec3(0, 1, 0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+Y")) {
            _trackballRotation = rotate(mat4 { 1 }, half_pi<float>(), vec3(1, 0, 0)) * rotate(mat4{1}, half_pi<float>(), vec3(0,1,0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+Z")) {
            _trackballRotation = mat3{1};
        }

        ImGui::Separator();

        ImGui::Checkbox("Ortho Projection", &_useOrthoProjection);
        ImGui::Checkbox("Draw Debug Lines", &_drawDebugLines);

        ImGui::Separator();

        ImGui::Text("AABBs");
        auto aabbDisplay = _aabbDisplay;
        ImGui::RadioButton("None", reinterpret_cast<int*>(&_aabbDisplay), 0);
        ImGui::RadioButton("Octree Graph", reinterpret_cast<int*>(&_aabbDisplay), 1);
        ImGui::RadioButton("March Nodes", reinterpret_cast<int*>(&_aabbDisplay), 2);
        if (aabbDisplay != _aabbDisplay) {
            aabbDisplayChanged();
        }

        ImGui::End();
    }

    void buildVolume()
    {
        // build a volume
        auto nThreads = std::thread::hardware_concurrency();
        std::cout << "Using " << nThreads << " threads to march _volume" << std::endl;

        std::vector<unowned_ptr<mc::ITriangleConsumer>> unownedTriangleConsumers;
        for (auto i = 0u; i < nThreads; i++) {
            _triangleConsumers.push_back(std::make_unique<mc::TriangleConsumer>());
            unownedTriangleConsumers.push_back(_triangleConsumers.back().get());
        }

        _volume = std::make_unique<mc::OctreeVolume>(64, _fuzziness, 4, unownedTriangleConsumers);
        _model = glm::translate(mat4 { 1 }, -vec3(_volume->bounds().center()));

        _volume->walkOctree([this](mc::OctreeVolume::Node* node) {
            auto bounds = node->bounds;
            bounds.inset(node->depth * OCTREE_NODE_VISUAL_INSET_FACTOR);
            _octreeAABBLineSegmentStorage.add(bounds, nodeColor(node->depth));
            return true;
        });

        buildDemo(_currentDemoIdx);

        marchVolume();
    }

    void buildDemo(int which)
    {
        std::cout << "Building demo \"" << Demos[which] << "\"" << std::endl;
        _volume->clear();
        _currentDemo = CreateDemo(Demos[which], _volume.get());
        marchVolume();
    }

    void aabbDisplayChanged()
    {
        marchVolume();
    }

    void marchVolume()
    {
        _marchStats.reset(_volume->depth());
        _octreeOccupiedAABBsLineSegmentStorage.clear();

        _volume->march(false, [this](mc::OctreeVolume::Node* node) {
            {
                // update the occupied aabb display
                auto bounds = node->bounds;
                bounds.inset(node->depth * OCTREE_NODE_VISUAL_INSET_FACTOR);
                _octreeOccupiedAABBsLineSegmentStorage.add(bounds, nodeColor(node->depth));
            }

            // update march stats
            _marchStats.nodesMarched++;
            _marchStats.voxelsMarched += iAABB(node->bounds).volume();
            _marchStats.nodesMarchedByDepth[node->depth]++;
        });

        for (const auto& tc : _triangleConsumers) {
            _marchStats.triangleCount += tc->numTriangles();
        }
    }

    const vec4 nodeColor(int atDepth) const
    {
        using namespace mc::util::color;
        static std::vector<vec4> nodeColors;

        auto depth = _volume->depth();
        if (nodeColors.size() != depth) {
            nodeColors.clear();
            const float hueStep = 360.0F / depth;
            for (auto i = 0U; i <= depth; i++) {
                const hsv hC { i * hueStep, 0.6F, 1.0F };
                const auto rgbC = Hsv2Rgb(hC);
                nodeColors.emplace_back(rgbC.r, rgbC.g, rgbC.b, mix<float>(0.6, 0.25, static_cast<float>(i) / depth));
            }
        }

        return nodeColors[atDepth];
    }

    void displayMarchStats()
    {
        auto maxVoxels = static_cast<int>(_volume->bounds().volume());
        std::cout << "marched " << _marchStats.voxelsMarched << "/" << maxVoxels
                  << " voxels (" << (static_cast<float>(_marchStats.voxelsMarched) / static_cast<float>(maxVoxels)) << ")"
                  << " numTriangles: " << _marchStats.triangleCount
                  << std::endl;

        for (auto i = 0U; i < _marchStats.nodesMarchedByDepth.size(); i++) {
            std::cout << "depth: " << i << "\t" << _marchStats.nodesMarchedByDepth[i]
                      << "/" << static_cast<int>(pow(8, i)) << " nodes"
                      << std::endl;
        }

        std::cout << std::endl;
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