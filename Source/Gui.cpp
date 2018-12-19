#include "Recorder.h"
#include <ImGui/imgui.h>
#include <SampleCode/SampleCode.h>
#include <GL/glew.h>

#include <memory>
#include <mutex>
#include <thread>

namespace Log = SampleCode::Log;
namespace Gui = SampleCode::Gui;

struct RecorderGui::Impl {
    std::mutex lock;
    ConfigCallback configCallback;
    ExitCallback exitCallback;
    std::unique_ptr<Gui::RenderLoop> renderLoop;

    AppConfig currentConfig;
    SampleSet currentSamples;
    SampleChanges currentChanges;

    bool requestShutdown = false;

private:
    RateMonitor renderMonitor;
    GLuint depthTextureId = 0;
    GLuint visibleTextureId = 0;
    GLuint leftInfraredTextureId = 0;
    GLuint rightInfraredTextureId = 0;

    bool renderCallback();
    void updateTexturesFromSamples(const AppConfig&, const SampleSet&, const SampleChanges&);
    void renderSamples();
    void renderControls(AppConfig&);
    void renderStatistics(const SampleSet&);

public:
    // impl taken by copy for thread-safety reasons
    static void threadMain(std::shared_ptr<Impl> impl) {
        Log::log("GUI thread starting");
        impl->renderLoop = std::make_unique<Gui::RenderLoop>();
        impl->renderLoop->main("Recorder", [&]() -> bool {
            return impl->renderCallback();
        });
        if (impl->depthTextureId != 0) {
            glDeleteTextures(1, &impl->depthTextureId);
        }
        if (impl->visibleTextureId != 0) {
            glDeleteTextures(1, &impl->visibleTextureId);
        }
        if (impl->leftInfraredTextureId != 0) {
            glDeleteTextures(1, &impl->leftInfraredTextureId);
        }
        if (impl->rightInfraredTextureId != 0) {
            glDeleteTextures(1, &impl->rightInfraredTextureId);
        }
        impl->exitCallback();
    }
};

void RecorderGui::Impl::updateTexturesFromSamples(const AppConfig& config, const SampleSet& samples, const SampleChanges& changes) {
    if (changes.depth && samples.depth.value.isValid()) {
        if (depthTextureId == 0) {
            glGenTextures(1, &depthTextureId);
        }
        Log::logv("Upload depth frame to texture %u", (unsigned)depthTextureId);
        Gui::cameraFrameToTexture(depthTextureId, samples.depth.value);
    }
    if (changes.visible && samples.visible.value.isValid()) {
        if (visibleTextureId == 0) {
            glGenTextures(1, &visibleTextureId);
        }
        Log::logv("Upload visible frame to texture %u", (unsigned)visibleTextureId);
        Gui::cameraFrameToTexture(visibleTextureId, samples.visible.value);
    }
    if (changes.infrared && samples.infrared.value.isValid()) {
        if (leftInfraredTextureId == 0) {
            glGenTextures(1, &leftInfraredTextureId);
        }
        if (rightInfraredTextureId == 0) {
            glGenTextures(1, &rightInfraredTextureId);
        }

        if (config.streaming.sensor.streamLeftInfrared == config.streaming.sensor.streamRightInfrared) {
            Log::logv("Upload stereo infrared frame to textures %u %u", (unsigned)leftInfraredTextureId, (unsigned)rightInfraredTextureId);
            // Both true: streaming both eyes
            // Both false: streaming OCC; don't know which eye(s) the frame is from, so assume both
            Gui::cameraFrameToTexture(leftInfraredTextureId, rightInfraredTextureId, samples.infrared.value);
        }
        else if (config.streaming.sensor.streamLeftInfrared) {
            Log::logv("Upload left infrared frame to texture %u", (unsigned)leftInfraredTextureId);
            Gui::cameraFrameToTexture(leftInfraredTextureId, samples.infrared.value);
        }
        else if (config.streaming.sensor.streamRightInfrared) {
            Log::logv("Upload right infrared frame to texture %u", (unsigned)rightInfraredTextureId);
            Gui::cameraFrameToTexture(rightInfraredTextureId, samples.infrared.value);
        }
    }
}

static const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
static const Gui::GridConfig gridConfig = {
    /* numCellsX */ 2,
    /* numCellsY */ 2,
    /* numTools */ 2,
    /* toolAreaWidth */ 250,
};

void RecorderGui::Impl::renderSamples() {
    ImGui::Begin("Depth", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 0, 0);
    if (depthTextureId != 0) {
        Gui::drawTextureInContentArea(depthTextureId);
    }
    ImGui::End();

    ImGui::Begin("Visible", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 1, 0);
    if (visibleTextureId != 0) {
        Gui::drawTextureInContentArea(visibleTextureId);
    }
    ImGui::End();

    ImGui::Begin("IR Left", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 0, 1);
    if (leftInfraredTextureId != 0) {
        Gui::drawTextureInContentArea(leftInfraredTextureId);
    }
    ImGui::End();

    ImGui::Begin("IR Right", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 1, 1);
    if (rightInfraredTextureId != 0) {
        Gui::drawTextureInContentArea(rightInfraredTextureId);
    }
    ImGui::End();
}

namespace {
    template<typename T> bool EnumRadioButton(const char *label, T *value, T ownValue) {
        int iv = (int)*value;
        bool ret = ImGui::RadioButton(label, &iv, (int)ownValue);
        *value = (T)iv;
        return ret;
    }
}

void RecorderGui::Impl::renderControls(AppConfig& config) {
    if (config.streaming.source == StreamingSource::Sensor) {
        ImGui::Begin("Sensor control", nullptr, windowFlags);
        Gui::layoutWindowAsGridTool(gridConfig, 0);

        ImGui::Checkbox("Depth", &config.streaming.sensor.streamDepth);
        ImGui::Checkbox("Visible", &config.streaming.sensor.streamVisible);
        ImGui::Checkbox("IR left", &config.streaming.sensor.streamLeftInfrared);
        ImGui::Checkbox("IR right", &config.streaming.sensor.streamRightInfrared);
        ImGui::Checkbox("Accelerometer", &config.streaming.sensor.streamAccel);
        ImGui::Checkbox("Gyroscope", &config.streaming.sensor.streamGyro);

        bool streamAll = ImGui::Button("All");
        ImGui::SameLine();
        bool streamNone = ImGui::Button("None");
        ImGui::SameLine();
        bool streamDepthAndVisibleOnly = ImGui::Button("D+V");
        ImGui::SameLine();
        bool streamImuOnly = ImGui::Button("IMU");
        ImGui::SameLine();
        bool streamDepthAndVisibleWithImu = ImGui::Button("D+V+IMU");
        ImGui::SameLine();
        bool streamBothInfrared = ImGui::Button("L+R");

        ImGui::Text("  Depth:");
        ImGui::SameLine();
        EnumRadioButton("VGA##depth", &config.streaming.sensor.depthResolution, DepthResolution::VGA);
        ImGui::SameLine();
        EnumRadioButton("Full##depth", &config.streaming.sensor.depthResolution, DepthResolution::Full);
        ImGui::Checkbox("Depth correction", &config.depthCorrection);
        ImGui::Checkbox("Frame sync", &config.streaming.frameSync);

        ImGui::End();

        if (streamAll) {
            config.streaming.sensor.streamDepth = true;
            config.streaming.sensor.streamVisible = true;
            config.streaming.sensor.streamLeftInfrared = true;
            config.streaming.sensor.streamRightInfrared = true;
            config.streaming.sensor.streamAccel = true;
            config.streaming.sensor.streamGyro = true;
        }
        else if (streamNone) {
            config.streaming.sensor.streamDepth = false;
            config.streaming.sensor.streamVisible = false;
            config.streaming.sensor.streamLeftInfrared = false;
            config.streaming.sensor.streamRightInfrared = false;
            config.streaming.sensor.streamAccel = false;
            config.streaming.sensor.streamGyro = false;
        }
        else if (streamDepthAndVisibleOnly) {
            config.streaming.sensor.streamDepth = true;
            config.streaming.sensor.streamVisible = true;
            config.streaming.sensor.streamLeftInfrared = false;
            config.streaming.sensor.streamRightInfrared = false;
            config.streaming.sensor.streamAccel = false;
            config.streaming.sensor.streamGyro = false;
        }
        else if (streamImuOnly) {
            config.streaming.sensor.streamDepth = false;
            config.streaming.sensor.streamVisible = false;
            config.streaming.sensor.streamLeftInfrared = false;
            config.streaming.sensor.streamRightInfrared = false;
            config.streaming.sensor.streamAccel = true;
            config.streaming.sensor.streamGyro = true;
        }
        else if (streamDepthAndVisibleWithImu) {
            config.streaming.sensor.streamDepth = true;
            config.streaming.sensor.streamVisible = true;
            config.streaming.sensor.streamLeftInfrared = false;
            config.streaming.sensor.streamRightInfrared = false;
            config.streaming.sensor.streamAccel = true;
            config.streaming.sensor.streamGyro = true;
        }
        else if (streamBothInfrared) {
            config.streaming.sensor.streamDepth = false;
            config.streaming.sensor.streamVisible = false;
            config.streaming.sensor.streamLeftInfrared = true;
            config.streaming.sensor.streamRightInfrared = true;
            config.streaming.sensor.streamAccel = false;
            config.streaming.sensor.streamGyro = false;
        }
    }
    else if (config.streaming.source == StreamingSource::OCC) {
        ImGui::Begin("OCC control", nullptr, windowFlags);
        Gui::layoutWindowAsGridTool(gridConfig, 0);

        ImGui::Checkbox("Stream OCC", &config.streaming.occ.streamOcc);
        ImGui::Checkbox("Depth correction", &config.depthCorrection);
        ImGui::Checkbox("Frame sync", &config.streaming.frameSync);

        ImGui::End();
    }
}

void RecorderGui::Impl::renderStatistics(const SampleSet& samples) {
    ImGui::Begin("Info", nullptr, windowFlags);
    Gui::layoutWindowAsGridTool(gridConfig, 1);
    ImGui::Text("  Depth FPS: %8.3f", samples.depth.rate);
    ImGui::Text("Visible FPS: %8.3f", samples.visible.rate);
    ImGui::Text("     IR FPS: %8.3f", samples.infrared.rate);
    ImGui::Text("  Accel FPS: %8.3f", samples.accel.rate);
    ImGui::Text("   Gyro FPS: %8.3f", samples.gyro.rate);
    ImGui::Text("Accel: [% .5f % .5f % .5f]", samples.accel.value.acceleration().x, samples.accel.value.acceleration().y, samples.accel.value.acceleration().z);
    ImGui::Text("Gyro:  [% .5f % .5f % .5f]", samples.gyro.value.rotationRate().x, samples.gyro.value.rotationRate().y, samples.gyro.value.rotationRate().z);
    ImGui::Text(" Render FPS: %8.3f", renderMonitor.rate());
    ImGui::End();
}

bool RecorderGui::Impl::renderCallback() {
    Log::logv("Render new frame");
    AppConfig preRenderConfig, postRenderConfig;
    SampleSet samples;
    SampleChanges changes;
    {
        std::unique_lock<std::mutex> u(lock);
        preRenderConfig = currentConfig;
        postRenderConfig = currentConfig;
        samples = currentSamples;
        changes = currentChanges;
        currentChanges = SampleChanges();
    }

    renderMonitor.tick();
    updateTexturesFromSamples(postRenderConfig, samples, changes);
    renderSamples();
    renderControls(postRenderConfig);
    renderStatistics(samples);

    bool shouldIssueCallback = false;
    {
        std::unique_lock<std::mutex> u(lock);
        if (requestShutdown) {
            // Instruct render loop to exit
            return false;
        }
        if (preRenderConfig != postRenderConfig) {
            shouldIssueCallback = !currentConfig.equiv(postRenderConfig);
            currentConfig = postRenderConfig;
            renderLoop->redrawNext();
        }
    }
    if (shouldIssueCallback) {
        configCallback(postRenderConfig);
    }
    return true;
}

RecorderGui::RecorderGui(const AppConfig& initialConfig, ConfigCallback configCallback, ExitCallback exitCallback)  {
    impl = std::make_shared<Impl>();
    impl->currentConfig = initialConfig;
    impl->configCallback = configCallback;
    impl->exitCallback = exitCallback;
    std::thread(Impl::threadMain, impl).detach();
}

RecorderGui::~RecorderGui() {
    std::unique_lock<std::mutex> u(impl->lock);
    impl->requestShutdown = true;
    impl->renderLoop->wake();
}

void RecorderGui::updateDisplayedConfig(const AppConfig& config) {
    std::unique_lock<std::mutex> u(impl->lock);
    impl->currentConfig = config;
    impl->renderLoop->wake();
}

void RecorderGui::updateSamples(const SampleSet& samples) {
    std::unique_lock<std::mutex> u(impl->lock);
    impl->currentChanges.registerSamples(impl->currentSamples, samples);
    impl->currentSamples = samples;
    impl->renderLoop->wake();
}

void RecorderGui::exit() {
    std::unique_lock<std::mutex> u(impl->lock);
    impl->requestShutdown = true;
    impl->renderLoop->wake();
}
