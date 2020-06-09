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
#include "demos.hpp"

using namespace glm;
using mc::util::AABB;
using mc::util::iAABB;
using mc::util::unowned_ptr;
using std::make_unique;
using std::unique_ptr;

//
// Constants
//

constexpr int kWindowWidth = 1440;
constexpr int kWindowHeight = 1440;
constexpr float kNearPlane = 0.1f;
constexpr float kFarPlane = 1000.0f;
constexpr float kFovDegrees = 50.0F;
constexpr float kOctreeNodeVisualInsetFactor = 0.0F;

//
// Materials
//

struct SkydomeMaterial {
private:
    GLuint _program = 0;
    GLint _uProjectionInverse = -1;
    GLint _uModelViewInverse = -1;
    GLint _uSkyboxSampler = -1;
    mc::util::TextureHandleRef _skyboxTex;

public:
    SkydomeMaterial(mc::util::TextureHandleRef skybox)
        : _skyboxTex(skybox)
    {
        using namespace mc::util;
        _program = CreateProgramFromFile("shaders/gl/skydome.glsl");
        _uProjectionInverse = glGetUniformLocation(_program, "uProjectionInverse");
        _uModelViewInverse = glGetUniformLocation(_program, "uModelViewInverse");
        _uSkyboxSampler = glGetUniformLocation(_program, "uSkyboxSampler");
    }

    SkydomeMaterial(const SkydomeMaterial& other) = delete;
    SkydomeMaterial(const SkydomeMaterial&& other) = delete;

    ~SkydomeMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }

    void bind(const mat4& projection, const mat4& modelview)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _skyboxTex->id());

        glUseProgram(_program);
        glUniformMatrix4fv(_uProjectionInverse, 1, GL_FALSE, value_ptr(inverse(projection)));
        glUniformMatrix4fv(_uModelViewInverse, 1, GL_FALSE, value_ptr(inverse(modelview)));
        glUniform1i(_uSkyboxSampler, 0);
    }
};

struct VolumeMaterial {
private:
    GLuint _program = 0;
    GLint _uMVP = -1;
    GLint _uModel = -1;
    GLint _uCameraPos = -1;
    GLint _uLightprobeSampler = -1;
    GLint _uAmbientLight;
    GLint _uReflectionMapSampler = -1;
    GLint _uReflectionMapMipLevels = -1;
    GLint _uShininess = -1;

    mc::util::TextureHandleRef _lightprobe;
    vec3 _ambientLight;
    mc::util::TextureHandleRef _reflectionMap;
    float _shininess;

public:
    VolumeMaterial(mc::util::TextureHandleRef lightProbe, vec3 ambientLight, mc::util::TextureHandleRef reflectionMap, float shininess)
        : _lightprobe(lightProbe)
        , _ambientLight(ambientLight)
        , _reflectionMap(reflectionMap)
        , _shininess(clamp(shininess, 0.0F, 1.0F))
    {
        using namespace mc::util;
        _program = CreateProgramFromFile("shaders/gl/volume.glsl");
        _uMVP = glGetUniformLocation(_program, "uMVP");
        _uModel = glGetUniformLocation(_program, "uModel");
        _uCameraPos = glGetUniformLocation(_program, "uCameraPosition");
        _uLightprobeSampler = glGetUniformLocation(_program, "uLightprobeSampler");
        _uAmbientLight = glGetUniformLocation(_program, "uAmbientLight");
        _uReflectionMapSampler = glGetUniformLocation(_program, "uReflectionMapSampler");
        _uReflectionMapMipLevels = glGetUniformLocation(_program, "uReflectionMapMipLevels");
        _uShininess = glGetUniformLocation(_program, "uShininess");
    }

    VolumeMaterial(const VolumeMaterial& other) = delete;
    VolumeMaterial(const VolumeMaterial&& other) = delete;

    ~VolumeMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }

    void setShininess(float s)
    {
        _shininess = clamp<float>(s, 0, 1);
    }

    float shininess() const { return _shininess; }

    void bind(const mat4& mvp, const mat4& model, const vec3& cameraPosition)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _lightprobe->id());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _reflectionMap->id());

        glUseProgram(_program);
        glUniformMatrix4fv(_uMVP, 1, GL_FALSE, value_ptr(mvp));
        glUniformMatrix4fv(_uModel, 1, GL_FALSE, value_ptr(model));
        glUniform3fv(_uCameraPos, 1, value_ptr(cameraPosition));
        glUniform1i(_uLightprobeSampler, 0);
        glUniform3fv(_uAmbientLight, 1, value_ptr(_ambientLight));
        glUniform1i(_uReflectionMapSampler, 1);
        glUniform1f(_uReflectionMapMipLevels, static_cast<float>(_reflectionMap->mipLevels()));
        glUniform1f(_uShininess, _shininess);
    }
};

struct LineMaterial {
private:
    GLuint _program = 0;
    GLint _uMVP = -1;

public:
    LineMaterial()
    {
        using namespace mc::util;
        _program = CreateProgramFromFile("shaders/gl/line.glsl");
        _uMVP = glGetUniformLocation(_program, "uMVP");
    }
    LineMaterial(const LineMaterial& other) = delete;
    LineMaterial(const LineMaterial&& other) = delete;

    ~LineMaterial()
    {
        if (_program > 0) {
            glDeleteProgram(_program);
        }
    }
    void bind(const mat4& mvp)
    {
        glUseProgram(_program);
        glUniformMatrix4fv(_uMVP, 1, GL_FALSE, value_ptr(mvp));
    }
};

//
// App
//

class App {
private:
    GLFWwindow* _window;
    mc::util::FpsCalculator _fpsCalculator;

    // render state
    std::unique_ptr<VolumeMaterial> _volumeMaterial;
    std::unique_ptr<LineMaterial> _lineMaterial;
    std::unique_ptr<SkydomeMaterial> _skydomeMaterial;
    std::vector<unique_ptr<mc::TriangleConsumer<mc::Vertex>>> _triangleConsumers;
    mc::util::LineSegmentBuffer _octreeAABBLineSegmentStorage;
    mc::util::LineSegmentBuffer _octreeOccupiedAABBsLineSegmentStorage;
    mc::util::LineSegmentBuffer _axes;
    mc::util::LineSegmentBuffer _debugLines;
    mc::TriangleConsumer<mc::util::VertexP3C4> _skydomeQuad;

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    vec2 _lastMousePosition { -1 };
    mat4 _model { 1 };
    mat3 _trackballRotation { 1 };

    // volume and samplers
    std::shared_ptr<mc::util::ThreadPool> _threadPool;
    unique_ptr<mc::OctreeVolume> _volume;
    std::vector<const char*> _demoNames;
    unique_ptr<Demo> _currentDemo;
    int _currentDemoIdx = 0;

    // app state
    enum class AABBDisplay : int {
        None,
        OctreeGraph,
        MarchNodes
    };

    bool _animate = false;
    float _animationTime = 1.4F;
    bool _running = true;
    bool _useOrthoProjection = false;
    AABBDisplay _aabbDisplay = AABBDisplay::None;
    bool _needsMarchVolume = true;
    float _fuzziness = 1.0F;
    float _aspect = 1;
    float _dolly = 1;
    bool _drawDebugLines = false;

    struct _MarchStats {
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

        _window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Marching Cubes", nullptr, nullptr);
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

        float volumeShininess = 0.75F;
        vec3 ambientLight { 0.0f, 0.0f, 0.0f };

        auto skyboxTexture = mc::util::LoadTextureCube("textures/sky", ".jpg");
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
        // Build static geometry (axes, skybox quad, etc)
        //

        {
            using V = decltype(_axes)::vertex_type;
            _axes.clear();
            _axes.add(V { vec3(0, 0, 0), vec4(1, 0, 0, 1) }, V { vec3(10, 0, 0), vec4(1, 0, 0, 1) });
            _axes.add(V { vec3(0, 0, 0), vec4(0, 1, 0, 1) }, V { vec3(0, 10, 0), vec4(0, 1, 0, 1) });
            _axes.add(V { vec3(0, 0, 0), vec4(0, 0, 1, 1) }, V { vec3(0, 0, 10), vec4(0, 0, 1, 1) });
        }

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
        // copy demo names to local storage so we can feed imgui::combo
        //

        _demoNames.resize(DemoRegistry.size());
        std::transform(DemoRegistry.begin(), DemoRegistry.end(), _demoNames.begin(),
            [](const DemoEntry& entry) { return entry.first; });

        //
        // build a volume
        //

        auto nThreads = std::thread::hardware_concurrency();
        std::cout << "Using " << nThreads << " threads to march _volume" << std::endl;
        _threadPool = std::make_shared<mc::util::ThreadPool>(nThreads, true);

        std::vector<unowned_ptr<mc::TriangleConsumer<mc::Vertex>>> unownedTriangleConsumers;
        for (auto i = 0u; i < nThreads; i++) {
            _triangleConsumers.push_back(make_unique<mc::TriangleConsumer<mc::Vertex>>());
            unownedTriangleConsumers.push_back(_triangleConsumers.back().get());
        }

        _volume = make_unique<mc::OctreeVolume>(64, _fuzziness, 4, _threadPool, unownedTriangleConsumers);
        _model = glm::translate(mat4 { 1 }, -vec3(_volume->bounds().center()));

        _volume->walkOctree([this](mc::OctreeVolume::Node* node) {
            auto bounds = node->bounds;
            bounds.inset(node->depth * kOctreeNodeVisualInsetFactor);
            _octreeAABBLineSegmentStorage.add(bounds, nodeColor(node->depth));
            return true;
        });

        //
        //  Build demo and march
        //

        buildDemo(_currentDemoIdx);
        marchVolume();
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
        if (_animate) {
            _animationTime += deltaT;
        }

        _currentDemo->step(_animationTime);

        if (_animate || _needsMarchVolume) {
            marchVolume();
            _needsMarchVolume = false;
        }
    }

    void drawFrame()
    {
        vec3 cameraPosition;
        mat4 model, view, projection;
        auto mvp = MVP(cameraPosition, model, view, projection);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // draw marching cubes output
        glDepthMask(GL_TRUE);
        _volumeMaterial->bind(mvp, _model, cameraPosition);
        for (auto& tc : _triangleConsumers) {
            tc->draw();
        }

        // draw skydome (after volume, it's depth is 1 in clip space)
        glDepthMask(GL_FALSE);
        _skydomeMaterial->bind(projection, (view * model));
        _skydomeQuad.draw();

        // draw lines
        glDepthMask(GL_FALSE);
        _lineMaterial->bind(mvp);

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

        if (ImGui::Combo("Demo", &_currentDemoIdx, _demoNames.data(), _demoNames.size())) {
            buildDemo(_currentDemoIdx);
        }

        ImGui::Separator();

        ImGui::Checkbox("Animate", &_animate);
        if (ImGui::InputFloat("Animation Time", &_animationTime, 0.01f, 0.1f, "%.2f")) {
            _needsMarchVolume = true;
        }

        ImGui::Separator();

        if (ImGui::SliderFloat("Fuzziness", &_fuzziness, 0, 5, "%.2f")) {
            _volume->setFuzziness(_fuzziness);
            _needsMarchVolume = true;
        }

        float shininess = _volumeMaterial->shininess();
        if (ImGui::SliderFloat("Shininess", &shininess, 0, 1)) {
            _volumeMaterial->setShininess(shininess);
        }

        ImGui::Separator();

        ImGui::Text("Reset Trackball Rotation");
        if (ImGui::Button("-X")) {
            _trackballRotation = rotate(mat4 { 1 }, +half_pi<float>(), vec3(0, 1, 0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+X")) {
            _trackballRotation = rotate(mat4 { 1 }, -half_pi<float>(), vec3(0, 1, 0));
        }
        ImGui::SameLine();
        if (ImGui::Button("-Y")) {
            _trackballRotation = rotate(mat4 { 1 }, -half_pi<float>(), vec3(1, 0, 0)) * rotate(mat4 { 1 }, half_pi<float>(), vec3(0, 1, 0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+Y")) {
            _trackballRotation = rotate(mat4 { 1 }, half_pi<float>(), vec3(1, 0, 0)) * rotate(mat4 { 1 }, half_pi<float>(), vec3(0, 1, 0));
        }
        ImGui::SameLine();
        if (ImGui::Button("-Z")) {
            _trackballRotation = rotate(mat4 { 1 }, pi<float>(), vec3(0, 1, 0));
        }
        ImGui::SameLine();
        if (ImGui::Button("+Z")) {
            _trackballRotation = mat3 { 1 };
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

    void buildDemo(int which)
    {
        std::cout << "Building demo \"" << DemoRegistry[which].first << "\"" << std::endl;
        _volume->clear();
        _currentDemo = DemoRegistry[which].second();
        _currentDemo->build(_volume.get());
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

        _volume->march([this](mc::OctreeVolume::Node* node) {
            {
                // update the occupied aabb display
                auto bounds = node->bounds;
                bounds.inset(node->depth * kOctreeNodeVisualInsetFactor);
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

    mat4 MVP(vec3& cameraPosition, mat4& model, mat4& view, mat4& projection) const
    {
        model = _model;

        // extract trackball Z and Y for building view matrix via lookAt
        auto trackballY = glm::vec3 { _trackballRotation[0][1], _trackballRotation[1][1], _trackballRotation[2][1] };
        auto trackballZ = glm::vec3 { _trackballRotation[0][2], _trackballRotation[1][2], _trackballRotation[2][2] };

        if (_useOrthoProjection) {
            auto bounds = _volume->bounds();
            auto size = length(bounds.size());

            auto scaleMin = 0.1F;
            auto scaleMax = 5.0F;
            auto scale = mix(scaleMin, scaleMax, pow<float>(_dolly, 2.5));

            auto width = scale * _aspect * size;
            auto height = scale * size;

            auto distance = kFarPlane / 2;
            cameraPosition = -distance * trackballZ;
            view = lookAt(cameraPosition, vec3(0), trackballY);

            projection = glm::ortho(-width / 2, width / 2, -height / 2, height / 2, kNearPlane, kFarPlane);
        } else {
            auto bounds = _volume->bounds();
            auto minDistance = 0.1F;
            auto maxDistance = length(bounds.size()) * 2;

            auto distance = mix(minDistance, maxDistance, pow<float>(_dolly, 2));
            cameraPosition = -distance * trackballZ;
            view = lookAt(cameraPosition, vec3(0), trackballY);

            projection = glm::perspective(radians(kFovDegrees), _aspect, kNearPlane, kFarPlane);
        }

        return projection * view * model;
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
