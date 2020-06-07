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

#include "../common/cubemap_blur.hpp"
#include "../common/post_processing_stack.hpp"
#include "FastNoise.h"
#include "camera.hpp"
#include "filters.hpp"
#include "materials.hpp"
#include "spring.hpp"
#include "terrain.hpp"

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
constexpr int HEIGHT = 1100;
constexpr float NEAR_PLANE = 0.1f;
constexpr float FAR_PLANE = 1000.0f;
constexpr float FOV_DEGREES = 50.0F;
constexpr float UI_SCALE = 2.0F;
constexpr float WORLD_RADIUS = 400;
constexpr int TERRAIN_GRID_SIZE = 3;

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
        _elapsedFrameTime = 0;
        while (isRunning()) {
            glfwPollEvents();

            // execute any queued operations
            mc::util::MainThreadQueue()->drain();

            // compute time delta and step simulation
            double now = glfwGetTime();
            double elapsed = now - lastTime;
            _elapsedFrameTime += elapsed;
            lastTime = now;
            step(static_cast<float>(now), static_cast<float>(elapsed));

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

            _framesRendered++;
            if (_framesRendered >= 60) {
                _currentFps = _framesRendered / _elapsedFrameTime;
                _framesRendered = 0;
                _elapsedFrameTime = 0;
            }
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
        bool working = false;
        _terrainGrid->forEach([&working](mc::util::unowned_ptr<TerrainChunk> chunk) {
            if (chunk->isWorking()) {
                working = true;
            }
        });

        return working;
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
        _terrainChunkSize = 128;

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
        const auto renderDistance = _terrainChunkSize * 1.5;

        _terrainMaterial = std::make_unique<TerrainMaterial>(
            0,
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
        _atmosphere->setFogWindSpeed(vec3(10, 0, 5));
        _atmosphere->setAlpha(0);

        _palettizer = _postProcessingFilters->push(std::make_unique<PalettizeFilter>(
            "Palettizer",
            ivec3(8, 8, 8),
            PalettizeFilter::ColorSpace::YUV));
        _palettizer->setAlpha(0);

        //
        // build a volume
        //

        const auto frequency = 1.0F / _terrainChunkSize;
        const auto terrainHeight = 32.0F;
        _fastNoise.SetNoiseType(FastNoise::Simplex);
        _fastNoise.SetFrequency(frequency);
        _fastNoise.SetFractalOctaves(3);

        _atmosphere->setFog(terrainHeight * 0.6, vec4(0.9, 0.9, 0.92, 0.5));

        //
        // build the terrain grid
        //

        class LumpyTerrainSource : public TerrainSampleSource {
        private:
            FastNoise& _noise;
            float _maxHeight;

        public:
            LumpyTerrainSource(FastNoise& noise, float maxHeight)
                : _noise(noise)
                , _maxHeight(maxHeight)
            {
            }
            float maxHeight() const override
            {
                return _maxHeight;
            }
            float sample(const vec3& world) const override
            {
                if (world.y < 1e-3F) {
                    return 1;
                }

                float noise2D = _noise.GetSimplex(world.x, world.z);
                float noise3D = _noise.GetSimplex(world.x * 11, world.y * 11, world.z * 11);
                float height = std::max(_maxHeight * noise2D, 0.0F);
                if (world.y < height) {
                    float a = (height - world.y) / height;
                    a = a * (a + 0.6F * noise3D);
                    return a;
                }

                return 0;
            }
        };

        class Greebler : public GreebleSource {
        private:
            FastNoise& _noise;

        public:
            Greebler(FastNoise& fn)
                : _noise(fn)
            {
            }

            int sampleStepSize() const override
            {
                return 15;
            }

            Sample sample(const vec3 world) const override
            {
                const float probability = (_noise.GetSimplex(world.x, world.z) + 1) * 0.5F; // map to [0,1]
                const uint64_t seed = static_cast<uint64_t>(12345 + probability * 678910);
                auto rng = rng_xorshift64 { seed };
                const vec3 offset { rng.nextFloat(-5, 5), rng.nextFloat(-5, 5), rng.nextFloat(-5, 5) };
                return Sample { probability, offset, seed };
            }

            std::unique_ptr<mc::IVolumeSampler> evaluate(const Sample& sample, const vec3& local) const override
            {
                if (sample.probability > 0.9) {
                    auto rng = rng_xorshift64 { sample.seed };
                    Tube::Config arch;
                    arch.axisOrigin = vec3 { local.x + sample.offset.x, 0, local.z + sample.offset.y };
                    arch.innerRadiusAxisOffset = vec3(0, rng.nextFloat(4, 10), 0);
                    arch.axisDir = normalize(vec3(rng.nextFloat(-0.6, 0.6), rng.nextFloat(-0.2, 0.2), 1));
                    arch.axisPerp = normalize(vec3(rng.nextFloat(-0.2, 0.2), 1, 0));
                    arch.length = rng.nextFloat(3, 7);
                    arch.innerRadius = rng.nextFloat(10, 15);
                    arch.outerRadius = rng.nextFloat(20, 35);
                    arch.frontFaceNormal = arch.axisDir;
                    arch.backFaceNormal = -arch.axisDir;
                    arch.cutAngleRadians = radians(rng.nextFloat(16, 32));
                    arch.material = kArchMaterial;
                    return std::make_unique<Tube>(arch);
                }
                return nullptr;
            }
        };

        std::unique_ptr<TerrainSampleSource> terrainSource = std::make_unique<LumpyTerrainSource>(_fastNoise, terrainHeight);
        std::unique_ptr<GreebleSource> greebleSource = std::make_unique<Greebler>(_fastNoise);
        _terrainGrid = std::make_unique<TerrainGrid>(TERRAIN_GRID_SIZE, _terrainChunkSize, std::move(terrainSource), std::move(greebleSource));

        auto pos = vec3(0, terrainHeight, 0);
        auto lookTarget = pos + vec3(0, 0, 1);
        _camera.lookAt(pos, lookTarget);

        _terrainGrid->march(_camera.getPosition(), _camera.getForward());

        std::cout << "initApp - Done" << std::endl;
    }

    void onResize(int width, int height)
    {
        _contextSize = ivec2 { width, height };
        glViewport(0, 0, width, height);
        _camera.updateProjection(width, height, FOV_DEGREES, NEAR_PLANE, FAR_PLANE);
    }

    void onKeyPress(int key, int scancode, int mods)
    {
        if (scancode == glfwGetKeyScancode(GLFW_KEY_ESCAPE)) {
            _running = false;
        }
    }

    void onKeyRelease(int key, int scancode, int mods)
    {
    }

    void onMouseMove(const vec2& pos, const vec2& delta)
    {
        if (_mouseButtonState[0]) {
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
        _postProcessingFilters->update(deltaT);
        updateCamera(deltaT);

        if (!_terrainGrid->isMarching()) {
            auto idx = _terrainGrid->worldToIndex(_camera.getPosition());
            auto centerIdx = _terrainGrid->getCenterChunk()->getIndex();
            auto shift = centerIdx - idx;
            if (shift != ivec2(0, 0)) {
                _terrainGrid->shift(centerIdx - idx);
                _terrainGrid->march(_camera.getPosition(), _camera.getForward());
            }
        }
    }

    void drawFrame()
    {
        const auto view = _camera.getView();
        const auto projection = _camera.getProjection();
        const auto frustum = _camera.getFrustum();

        _atmosphere->setCameraState(_camera.getPosition(), projection, view, NEAR_PLANE, FAR_PLANE);

        _postProcessingFilters->execute(_contextSize / std::max(_pixelScale, 1), _contextSize, [=]() {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // draw volumes
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);

            _terrainGrid->forEach([&](mc::util::unowned_ptr<TerrainChunk> chunk) {
                if (frustum.intersect(chunk->getBounds()) != Frustum::Intersection::Outside) {
                    const auto modelTranslation = chunk->getWorldOrigin();
                    _terrainMaterial->bind(modelTranslation, view, projection, _camera.getPosition());
                    for (auto& buffer : chunk->getGeometry()) {
                        buffer->draw();
                    }
                }
            });
        });

        glViewport(0, 0, _contextSize.x, _contextSize.y);

        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        // draw optional markers, aabbs, etc
        if (_drawOctreeAABBs || _drawMarkers) {
            if (_drawMarkers) {
                // draw the origin of our scene
                _lineMaterial->bind(projection * view * mat4 { 1 });
                _axisMarker.draw();
            }

            _terrainGrid->forEach([&](mc::util::unowned_ptr<TerrainChunk> chunk) {
                const auto model = translate(mat4 { 1 }, chunk->getWorldOrigin());
                _lineMaterial->bind(projection * view * model);
                if (_drawMarkers) {
                    chunk->getBoundingLineBuffer().draw();
                }
                if (_drawOctreeAABBs) {
                    chunk->getAabbLineBuffer().draw();
                }
            });
        }

        glDepthMask(GL_TRUE);
    }

    void drawGui()
    {
        ImGui::Begin("Demo window");
        ImGui::LabelText("FPS", "%03.0f", _currentFps);

        double avgMarchDuration = 0;
        _terrainGrid->forEach([&avgMarchDuration](mc::util::unowned_ptr<TerrainChunk> chunk) {
            avgMarchDuration += chunk->getLastMarchDurationSeconds();
        });
        avgMarchDuration /= _terrainGrid->getCount();
        ImGui::LabelText("march duration", "%.2fs", avgMarchDuration);

        ImGui::Separator();
        ImGui::Checkbox("Draw AABBs", &_drawOctreeAABBs);
        ImGui::Checkbox("Draw Markers", &_drawMarkers);

        ImU32 pixelScale = _pixelScale;
        ImU32 pixelScaleStep = 1;
        if (ImGui::InputScalar("Pixel Scale", ImGuiDataType_U32, &pixelScale, &pixelScaleStep, nullptr)) {
            _pixelScale = max<int>(pixelScale, 1);
        }

        bool roundWorld = _terrainMaterial->getWorldRadius() > 0;
        if (ImGui::Checkbox("Round World", &roundWorld)) {
            _terrainMaterial->setWorldRadius(roundWorld ? WORLD_RADIUS : 0);
        }

        bool drawAtmosphere = _atmosphere->getAlpha() > 0.5F;
        if (ImGui::Checkbox("Draw Atmosphere", &drawAtmosphere)) {
            _atmosphere->setAlpha(drawAtmosphere ? 1 : 0);
        }

        float paletteAlpha = _palettizer->getAlpha();
        if (ImGui::SliderFloat("Palettize", &paletteAlpha, 0, 1, "%.2f")) {
            _palettizer->setAlpha(paletteAlpha);
        }

        ImGui::End();
    }

    void updateCamera(float deltaT)
    {
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
            _camera.rotateBy(0, +lookSpeed);
        }

        if (isKeyDown(GLFW_KEY_LEFT)) {
            _camera.rotateBy(-lookSpeed, 0);
        }

        if (isKeyDown(GLFW_KEY_RIGHT)) {
            _camera.rotateBy(+lookSpeed, 0);
        }

        _camera.updateFrustum();
    }

    bool isKeyDown(int scancode) const
    {
        return _pressedkeyScancodes.find(glfwGetKeyScancode(scancode)) != _pressedkeyScancodes.end();
    }

private:
    // app state
    GLFWwindow* _window;
    double _elapsedFrameTime = 0;
    int _framesRendered = 0;
    double _currentFps = 0;
    bool _running = false;
    ivec2 _contextSize { WIDTH, HEIGHT };

    // input state
    bool _mouseButtonState[3] = { false, false, false };
    std::set<int> _pressedkeyScancodes;
    vec2 _lastMousePosition { -1 };

    // render state
    Camera _camera;
    std::unique_ptr<TerrainMaterial> _terrainMaterial;
    std::unique_ptr<LineMaterial> _lineMaterial;
    mc::util::LineSegmentBuffer _axisMarker;
    std::unique_ptr<post_processing::FilterStack> _postProcessingFilters;
    mc::util::unowned_ptr<AtmosphereFilter> _atmosphere;
    mc::util::unowned_ptr<PalettizeFilter> _palettizer;

    // user input state
    int _pixelScale = 2;
    bool _drawOctreeAABBs = false;
    bool _drawMarkers = true;

    // demo state
    int _terrainChunkSize = 0;
    std::unique_ptr<TerrainGrid> _terrainGrid;
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
