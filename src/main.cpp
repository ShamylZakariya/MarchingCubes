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

#include "lines.hpp"
#include "marching_cubes.hpp"
#include "storage.hpp"
#include "triangle_soup.hpp"
#include "util.hpp"
#include "volume.hpp"
#include "volume_samplers.hpp"

//
// Constants
//

const int WIDTH = 1440;
const int HEIGHT = 900;
const float NEAR_PLANE = 0.1f;
const float FAR_PLANE = 1000.0f;
const float FOV_DEGREES = 50.0F;
const float OCTREE_NODE_INSET_FACTOR = 0.0625F;

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

class OpenGLCubeApplication {
public:
    OpenGLCubeApplication()
    {
        initWindow();
        initGl();
        buildVolume();
    }

    ~OpenGLCubeApplication() = default;

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
    ProgramState _volumeProgram, _lineProgram;
    std::vector<std::unique_ptr<ITriangleConsumer>> _triangleConsumers;

    bool _mouseButtonState[3] = { false, false, false };
    vec2 _lastMousePosition { -1 };

    mat4 _model { 1 };
    mat4 _trackballRotation { 1 };

    std::unique_ptr<OctreeVolume> _volume;
    unowned_ptr<SphereVolumeSampler> _mainShere;
    unowned_ptr<SphereVolumeSampler> _secondaryShere;
    unowned_ptr<BoundedPlaneVolumeSampler> _plane;
    std::unique_ptr<mc::ThreadedMarcher> _marcher;
    LineSegmentBuffer _octreeAABBLineSegmentStorage;
    LineSegmentBuffer _octreeOccupiedAABBsLineSegmentStorage;

    bool _animateVolume = false;
    bool _running = true;
    bool _useOrthoProjection = true;

    enum class AABBDisplay : int {
        None,
        OctreeGraph,
        MarchNodes
    };

    AABBDisplay _aabbDisplay = AABBDisplay::None;

    enum class MarchTechnique : int {
        ThreadedMarcher = 0,
        OctreeVolume = 1
    };

    MarchTechnique _marchTechnique = MarchTechnique::OctreeVolume;

    bool _needsMarchVolume = false;
    float _fuzziness = 1.0F;
    float _aspect = 1;
    float _dolly = 1;
    float _animationPhase = 0;

    std::vector<vec4> _nodeColors;
    std::vector<OctreeVolume::Node*> _nodesMarched;

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
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            app->onResize(width, height);
        });

        glfwSetKeyCallback(_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            if (ImGui::GetIO().WantCaptureKeyboard) {
                return;
            }

            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
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

            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
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

            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
            app->onMouseWheel(vec2(xOffset, yOffset));
        });

        glfwSetCursorPosCallback(_window, [](GLFWwindow* window, double xPos, double yPos) {
            auto app = reinterpret_cast<OpenGLCubeApplication*>(glfwGetWindowUserPointer(window));
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
        util::CheckGlError("drawFrame");
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const auto model = _trackballRotation * _model;
        const auto mvp = [this, &model]() {
            if (_useOrthoProjection)
            {
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
            }
            else
            {
                auto bounds = _volume->bounds();
                auto minDistance = 0.1F;
                auto maxDistance = length(bounds.size()) * 2;

                auto distance = mix(minDistance, maxDistance, pow<float>(_dolly, 2));
                auto view = lookAt(-distance * vec3(0, 0, 1), vec3(0), vec3(0, 1, 0));

                auto proj = glm::perspective(radians(FOV_DEGREES), _aspect, NEAR_PLANE, FAR_PLANE);
                return proj * view * model;
            }
        }();

        glUseProgram(_volumeProgram.program);
        glUniformMatrix4fv(_volumeProgram.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
        glUniformMatrix4fv(_volumeProgram.uniformLocModel, 1, GL_FALSE, value_ptr(model));

        glDepthMask(GL_TRUE);
        for (auto& tc : _triangleConsumers) {
            tc->draw();
        }

        switch (_aabbDisplay) {
        case AABBDisplay::None:
            break;
        case AABBDisplay::OctreeGraph:
            glDepthMask(GL_FALSE);
            glUseProgram(_lineProgram.program);
            glUniformMatrix4fv(_lineProgram.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
            glUniformMatrix4fv(_lineProgram.uniformLocModel, 1, GL_FALSE, value_ptr(model));
            _octreeAABBLineSegmentStorage.draw();
            break;
        case AABBDisplay::MarchNodes:
            glDepthMask(GL_FALSE);
            glUseProgram(_lineProgram.program);
            glUniformMatrix4fv(_lineProgram.uniformLocMVP, 1, GL_FALSE, value_ptr(mvp));
            glUniformMatrix4fv(_lineProgram.uniformLocModel, 1, GL_FALSE, value_ptr(model));
            _octreeOccupiedAABBsLineSegmentStorage.draw();
            break;
        }

        glDepthMask(GL_TRUE);
    }

    void drawGui()
    {
        ImGui::Begin("Demo window");

        ImGui::LabelText("FPS", "%2.1f", static_cast<float>(_fpsCalculator.getFps()));

        ImGui::Checkbox("Animate", &_animateVolume);
        if (!_animateVolume) {
            ImGui::SameLine();
            _needsMarchVolume = ImGui::Button("March Once");
        }

        if (ImGui::InputFloat("Animation Phase", &_animationPhase, 0.01F, 0.1F)) {
            _needsMarchVolume = true;
        }

        ImGui::Separator();
        ImGui::Text("March Technique");
        auto technique = _marchTechnique;
        ImGui::RadioButton("ThreadedMarcher", reinterpret_cast<int*>(&_marchTechnique), 0);
        ImGui::RadioButton("OctreeVolumeMarcher", reinterpret_cast<int*>(&_marchTechnique), 1);
        if (_marchTechnique != technique) {
            marchTechniqueChanged();
        }

        ImGui::Separator();
        if (ImGui::SliderFloat("Fuzziness", &_fuzziness, 0, 5, "%.2f")) {
            _volume->setFuzziness(_fuzziness);
            _needsMarchVolume = true;
        }

        ImGui::Separator();
        ImGui::Checkbox("Ortho Projection", &_useOrthoProjection);

        ImGui::Text("Reset Trackball Rotation");
        if (ImGui::Button("+X")) {
            _trackballRotation = rotate(mat4{1}, 0.0F, vec3(1,0,0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+Y")) {
            _trackballRotation = rotate(mat4{1}, half_pi<float>(), vec3(1,0,0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+Z")) {
            _trackballRotation = rotate(mat4{1}, half_pi<float>(), vec3(0,1,0));
        }

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

        buildNodeColors(_volume->depth());

        _volume->walkOctree([this](OctreeVolume::Node* node) {
            auto bounds = node->bounds;
            bounds.inset(node->depth * OCTREE_NODE_INSET_FACTOR);
            AppendAABB(bounds, _octreeAABBLineSegmentStorage, _nodeColors[node->depth]);
            return true;
        });

        // TODO: when OctreeVolume works remove this fallback ThreadedMarcher
        _marcher = std::make_unique<mc::ThreadedMarcher>(*(_volume.get()), unownedTriangleConsumers);

        // build samplers to populate the volume

        auto size = vec3(_volume->size());
        auto center = size / 2.0F;
        auto mainSphereRadius = size.x * 0.4F;
        auto secondarySphereRadius = mainSphereRadius * 0.2F;

        _mainShere = _volume->add(std::make_unique<SphereVolumeSampler>(
            center, mainSphereRadius, IVolumeSampler::Mode::Additive));

        if (true) {
            _secondaryShere = _volume->add(std::make_unique<SphereVolumeSampler>(
                center + vec3(mainSphereRadius, 0, 0), secondarySphereRadius, IVolumeSampler::Mode::Additive));

            _plane = _volume->add(std::make_unique<BoundedPlaneVolumeSampler>(
                center, vec3(0, 1, 0), 8, IVolumeSampler::Mode::Subtractive));
        }

        marchVolume();
    }

    void aabbDisplayChanged()
    {
        marchVolume();
    }

    void marchTechniqueChanged()
    {
        // clears storage
        for (const auto& tc : _triangleConsumers) {
            tc->start();
            tc->finish();
        }
        marchVolume();
    }

    void marchVolume()
    {
        switch (_marchTechnique) {
        case MarchTechnique::ThreadedMarcher:
            _marcher->march(mat4(1), false);
            break;
        case MarchTechnique::OctreeVolume:
            _volume->march(mat4(1), false, &_nodesMarched);
            break;
        }

        _octreeOccupiedAABBsLineSegmentStorage.clear();
        if (_aabbDisplay == AABBDisplay::MarchNodes) {
            for (auto node : _nodesMarched) {
                auto bounds = node->bounds;
                bounds.inset(node->depth * OCTREE_NODE_INSET_FACTOR);
                AppendAABB(bounds, _octreeOccupiedAABBsLineSegmentStorage, _nodeColors[node->depth]);
            }
        }
    }

    void buildNodeColors(int depth)
    {
        // generate AABBs for debug
        _nodeColors.clear();
        const float hueStep = 360.0F / depth;
        for (int i = 0; i <= depth; i++) {
            const util::color::hsv hC { i * hueStep, 0.6F, 1.0F };
            const auto rgbC = util::color::Hsv2Rgb(hC);
            _nodeColors.emplace_back(rgbC.r, rgbC.g, rgbC.b, mix<float>(0.6, 0.25, static_cast<float>(i) / depth));
        }
    }

    void displayMarchStats()
    {
        auto maxVoxels = static_cast<int>(_volume->bounds().volume());
        int voxelsMarched = 0;
        std::vector<int> nodesMarchedByDepth;
        _volume->marchStats(voxelsMarched, nodesMarchedByDepth, _nodesMarched);

        std::cout << "marched " << voxelsMarched << "/" << maxVoxels
                  << " voxels (" << (static_cast<float>(voxelsMarched) / static_cast<float>(maxVoxels)) << ")"
                  << std::endl;

        for (auto i = 0U; i < nodesMarchedByDepth.size(); i++) {
            std::cout << "depth: " << i << "\t" << nodesMarchedByDepth[i]
                      << "/" << static_cast<int>(pow(8, i)) << " nodes"
                      << std::endl;
        }

        std::cout << std::endl;
    }

    void examineOctree()
    {
        auto indenter = [](int count) {
            std::string indent = "";
            for (int i = 0; i < count; i++) {
                indent += "\t";
            }
            return indent;
        };

        _volume->walkOctree([&indenter](OctreeVolume::Node* node) {
            std::cout << indenter(node->depth) << "(" << node->childIdx << " @ " << node->depth << ")"
                      << " march: " << std::boolalpha << node->march
                      << " empty: " << std::boolalpha << node->empty << std::endl;
            return true;
        });
    }
};

//
// Main
//

int main()
{
    OpenGLCubeApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
