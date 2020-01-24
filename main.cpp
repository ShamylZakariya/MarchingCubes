//
//  main.cpp
//  MarchingCubes
//
//  Created by Shamyl Zakariya on 11/23/19.
//  Copyright © 2019 Shamyl Zakariya. All rights reserved.
//

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <epoxy/gl.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <lines.hpp>
#include <marching_cubes.hpp>
#include <storage.hpp>
#include <triangle_soup.hpp>
#include <util.hpp>
#include <volume.hpp>
#include <volume_samplers.hpp>

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

    ~ProgramState()
    {
        if (program > 0) {
            glDeleteProgram(program);
        }
    }

    void build(const std::string& vertPath, const std::string& fragPath)
    {
        auto vertSrc = util::ReadFile(vertPath);
        auto fragSrc = util::ReadFile(fragPath);
        program = util::CreateProgram(vertSrc.c_str(), fragSrc.c_str());
        uniformLocMVP = glGetUniformLocation(program, "uMVP");
        uniformLocModel = glGetUniformLocation(program, "uModel");
    }
};

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
        ImGui::GetIO().Fonts->AddFontFromFileTTF("./fonts/ConsolaMono.ttf", 24);

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
    util::FpsCalculator _fpsCalculator;

    // render state
    ProgramState _volumeProgram, _lineProgram;
    std::vector<std::unique_ptr<ITriangleConsumer>> _triangleConsumers;
    LineSegmentBuffer _octreeAABBLineSegmentStorage;
    LineSegmentBuffer _octreeOccupiedAABBsLineSegmentStorage;
    LineSegmentBuffer _axes;
    LineSegmentBuffer _debugLines;

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    vec2 _lastMousePosition { -1 };
    mat4 _model { 1 };
    mat4 _trackballRotation { 1 };

    // volume and samplers
    std::unique_ptr<OctreeVolume> _volume;
    unowned_ptr<RectangularPrismVolumeSampler> _rectPrism;
    unowned_ptr<BoundedPlaneVolumeSampler> _plane;

    // app state
    enum class AABBDisplay : int {
        None,
        OctreeGraph,
        MarchNodes
    };

    bool _animateVolume = false;
    bool _running = true;
    bool _useOrthoProjection = true;
    AABBDisplay _aabbDisplay = AABBDisplay::MarchNodes;
    bool _needsMarchVolume = true;
    float _fuzziness = 1.0F;
    float _aspect = 1;
    float _dolly = 1;
    float _animationPhase = 0.410F;
    bool _drawMarchedVolume = true;
    bool _drawDebugLines = true;

    struct MarchInfo {
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
            mat4 xRot = rotate(mat4(1), -1 * delta.y * trackballSpeed, vec3(1, 0, 0));
            mat4 yRot = rotate(mat4(1), 1 * delta.x * trackballSpeed, vec3(0, 1, 0));
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
            _animationPhase = now * 0.2F;
        }

        float i = 0;
        _animationPhase = std::modf(_animationPhase, &i);

        if (_rectPrism) {
            float angle = static_cast<float>(M_PI * _animationPhase);
            _rectPrism->setRotation(rotate(mat4 { 1 }, angle, vec3(0, 0, 1)));
        }

        if (_plane) {
            float angle = static_cast<float>(M_PI * _animationPhase);
            vec3 normal { rotate(mat4(1), angle, vec3(1, 0, 0)) * vec4(0, 1, 0, 1) };
            _plane->setPlaneNormal(normal);
        }

        if (_animateVolume || _needsMarchVolume) {
            marchVolume();
            _needsMarchVolume = false;
        }
    }

    void drawFrame()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto model = _trackballRotation * _model;
        const auto mvp = [this, &model]() {
            if (_useOrthoProjection) {
                auto bounds = _volume->bounds();
                auto size = length(bounds.size());

                auto scaleMin = 0.1F;
                auto scaleMax = 5.0F;
                auto scale = mix(scaleMin, scaleMax, pow<float>(_dolly, 2.5));

                auto width = scale * _aspect * size;
                auto height = scale * size;

                auto distance = FAR_PLANE / 2;
                auto view = lookAt(-distance * vec3(0, 0, 1), vec3(0), vec3(0, 1, 0));

                auto proj = glm::ortho(-width / 2, width / 2, -height / 2, height / 2, NEAR_PLANE, FAR_PLANE);
                return proj * view * model;
            } else {
                auto bounds = _volume->bounds();
                auto minDistance = 0.1F;
                auto maxDistance = length(bounds.size()) * 2;

                auto distance = mix(minDistance, maxDistance, pow<float>(_dolly, 2));
                auto view = lookAt(-distance * vec3(0, 0, 1), vec3(0), vec3(0, 1, 0));

                auto proj = glm::perspective(radians(FOV_DEGREES), _aspect, NEAR_PLANE, FAR_PLANE);
                return proj * view * model;
            }
        }();

        if (_drawMarchedVolume) {
            glUseProgram(_volumeProgram.program);
            glUniformMatrix4fv(_volumeProgram.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
            glUniformMatrix4fv(_volumeProgram.uniformLocModel, 1, GL_FALSE, value_ptr(model));

            glDepthMask(GL_TRUE);
            for (auto& tc : _triangleConsumers) {
                tc->draw();
            }
        }

        glDepthMask(GL_FALSE);
        glUseProgram(_lineProgram.program);
        glUniformMatrix4fv(_lineProgram.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
        glUniformMatrix4fv(_lineProgram.uniformLocModel, 1, GL_FALSE, value_ptr(model));

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
            _debugLines.add(_rectPrism->bounds(), vec4(1, 1, 0, 1));
            _rectPrism->addTo(_debugLines, vec4(0, 1, 1, 1));

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

        ImGui::Checkbox("Animate", &_animateVolume);
        if (!_animateVolume) {
            ImGui::SameLine();
            _needsMarchVolume = ImGui::Button("March Once");
        }

        if (ImGui::InputFloat("Animation Phase", &_animationPhase, 0.01F, 0.1F)) {
            _needsMarchVolume = true;
        }

        ImGui::Separator();
        if (ImGui::SliderFloat("Fuzziness", &_fuzziness, 0, 5, "%.2f")) {
            _volume->setFuzziness(_fuzziness);
            _needsMarchVolume = true;
        }

        ImGui::Separator();

        ImGui::Text("Reset Trackball Rotation");
        if (ImGui::Button("+X")) {
            _trackballRotation = rotate(mat4 { 1 }, 0.0F, vec3(1, 0, 0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+Y")) {
            _trackballRotation = rotate(mat4 { 1 }, half_pi<float>(), vec3(1, 0, 0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+Z")) {
            _trackballRotation = rotate(mat4 { 1 }, half_pi<float>(), vec3(0, 1, 0));
        }

        ImGui::Separator();

        ImGui::Checkbox("Ortho Projection", &_useOrthoProjection);
        ImGui::Checkbox("Draw Marched Volume", &_drawMarchedVolume);
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

        std::vector<unowned_ptr<ITriangleConsumer>> unownedTriangleConsumers;
        for (auto i = 0u; i < nThreads; i++) {
            _triangleConsumers.push_back(std::make_unique<TriangleConsumer>());
            unownedTriangleConsumers.push_back(_triangleConsumers.back().get());
        }

        _volume = std::make_unique<OctreeVolume>(64, _fuzziness, 4, unownedTriangleConsumers);
        _model = glm::translate(mat4 { 1 }, -vec3(_volume->bounds().center()));

        _volume->walkOctree([this](OctreeVolume::Node* node) {
            auto bounds = node->bounds;
            bounds.inset(node->depth * OCTREE_NODE_VISUAL_INSET_FACTOR);
            _octreeAABBLineSegmentStorage.add(bounds, nodeColor(node->depth));
            return true;
        });

        // build samplers to populate the volume

        auto size = vec3(_volume->size());
        auto center = size / 2.0F;

        _rectPrism = _volume->add(std::make_unique<RectangularPrismVolumeSampler>(
            center, vec3(10, 1, 5), mat3 { 1 }, IVolumeSampler::Mode::Additive));

        if (false) {
            _plane = _volume->add(std::make_unique<BoundedPlaneVolumeSampler>(
                center, vec3(0, 1, 0), 8, IVolumeSampler::Mode::Subtractive));
        }

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

        _volume->march(mat4(1), false, [this](OctreeVolume::Node* node) {
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
        static std::vector<vec4> nodeColors;

        auto depth = _volume->depth();
        if (nodeColors.size() != depth) {
            nodeColors.clear();
            const float hueStep = 360.0F / depth;
            for (auto i = 0U; i <= depth; i++) {
                const util::color::hsv hC { i * hueStep, 0.6F, 1.0F };
                const auto rgbC = util::color::Hsv2Rgb(hC);
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
