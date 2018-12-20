// header files inside of playground source code
#include "Playground.h"
#include "PlotVar.h"

// external libraries
#include <ImGui/imgui.h>
#include <SampleCode/SampleCode.h>
#include <GL/glew.h>

// standard library
#include <memory>
#include <mutex>
#include <thread>
#include <stdio.h>

// namespaces from the SampleCode library in the Structure SDK
namespace Log = SampleCode::Log;
namespace Gui = SampleCode::Gui;

/**
 * A simple data structure for holding a log that is readable, clerable, and
 * copyable by the user inside of the GUI.
 */
struct AppLog
{
    ImGuiTextBuffer Buf; // contents of the log in a text buffer
    bool ScrollToBottom; // whether the log auto-scrolls down

    /**< Clear the log window of all text contents. */
    void Clear() { Buf.clear(); }

    /**
     * @brief Add a string to the log window inside of the GUI.
     *
     * This method appends a string to the buffer for the log window
     * and sets the flag to scroll the window down to the latest
     * message.
     *
     * @param fmt - the printf-formatted string for the log window
     * @param ... - the paramters to insert into the log output
     */
    void AddLog(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        Buf.appendf(fmt, args);
        va_end(args);
        ScrollToBottom = true;
    }

    /**
     * @brief Draw the log output window.
     *
     * This method uses ImGui to create a small resizable and movable window
     * inside of the application for log output. This log window contains a
     * scrolling text box that updates as new logs are added using the `AddLog`
     * method.
     *
     * @param title - the title of the window
     * @param p_opened - whether the window is opened
     */
    void Draw(const char *title, bool *p_opened = NULL)
    {
        // set window 500x400 in size
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiSetCond_FirstUseEver);

        ImGui::Begin(title, p_opened); // create the window

        // create a "Clear" button that wipes all displayed text
        if (ImGui::Button("Clear"))
            Clear();

        ImGui::SameLine(); // display buttons inline

        // create a "Copy" button to copy text to clipboard
        bool copy = ImGui::Button("Copy");

        ImGui::Separator();

        // begin the text body
        ImGui::BeginChild("scrolling");
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));

        // handle clicks on the copy button
        if (copy)
            ImGui::LogToClipboard();

        // display the text buffor
        ImGui::TextUnformatted(Buf.begin());

        // scroll to the bottom if needed
        if (ScrollToBottom)
            ImGui::SetScrollHere(1.0f);
        ScrollToBottom = false;

        // end window creation
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::End();
    }
};

/**
 * @brief Data structure holding the GUI thread for the Structure Recorder.
 *
 */
struct RecorderGui::Impl
{
    std::mutex lock;                             // lock for UI thread
    ConfigCallback configCallback;               // callback executed on render
    ExitCallback exitCallback;                   // callback executed on quit
    std::unique_ptr<Gui::RenderLoop> renderLoop; // RenderLoop from SampleCode library

    AppConfig currentConfig;      // the configuration for the next samples
    SampleSet currentSamples;     // the most recent data samples
    SampleChanges currentChanges; // holds boolean for whether a sample changed

    bool requestShutdown = false; // quit the thread if set to true

  private:
    RateMonitor renderMonitor; // keep track of render rate (fps)

    // GUI elements for renderring different image data (depth, visible, and IR)
    GLuint depthTextureId = 0;
    GLuint visibleTextureId = 0;
    GLuint leftInfraredTextureId = 0;
    GLuint rightInfraredTextureId = 0;

    // two log windows for containing accelerometer and gyroscope data
    AppLog accelLog;
    AppLog gyroLog;

    // private methods used for renderring
    bool renderCallback();
    void updateTexturesFromSamples(const AppConfig &, const SampleSet &, const SampleChanges &);
    void renderSamples();
    void renderControls(AppConfig &);
    void setStyles();
    void renderStatistics(const SampleSet &);

  public:
    // impl taken by copy for thread-safety reasons
    static void threadMain(std::shared_ptr<Impl> impl)
    {
        Log::log("GUI thread starting");
        impl->renderLoop = std::make_unique<Gui::RenderLoop>();
        impl->renderLoop->main("Recorder", [&]() -> bool {
            return impl->renderCallback();
        });
        if (impl->depthTextureId != 0)
        {
            glDeleteTextures(1, &impl->depthTextureId);
        }
        if (impl->visibleTextureId != 0)
        {
            glDeleteTextures(1, &impl->visibleTextureId);
        }
        if (impl->leftInfraredTextureId != 0)
        {
            glDeleteTextures(1, &impl->leftInfraredTextureId);
        }
        if (impl->rightInfraredTextureId != 0)
        {
            glDeleteTextures(1, &impl->rightInfraredTextureId);
        }
        impl->exitCallback();
    }
};

/**
 * @brief Update the texture elements with the images from the different cameras
 *
 * Structure Core provides images with visible light
 *
 * @param config - the current configuration (which options are selected)
 * @param samples - the latest sample set from Structure Core
 * @param changes - which sample values have changed
 */
void RecorderGui::Impl::updateTexturesFromSamples(const AppConfig &config, const SampleSet &samples, const SampleChanges &changes)
{
    if (changes.depth && samples.depth.value.isValid())
    {
        if (depthTextureId == 0)
        {
            glGenTextures(1, &depthTextureId);
        }
        Log::logv("Upload depth frame to texture %u", (unsigned)depthTextureId);
        Gui::cameraFrameToTexture(depthTextureId, samples.depth.value);
    }
    if (changes.visible && samples.visible.value.isValid())
    {
        if (visibleTextureId == 0)
        {
            glGenTextures(1, &visibleTextureId);
        }
        Log::logv("Upload visible frame to texture %u", (unsigned)visibleTextureId);
        Gui::cameraFrameToTexture(visibleTextureId, samples.visible.value);
    }
    if (changes.infrared && samples.infrared.value.isValid())
    {
        if (leftInfraredTextureId == 0)
        {
            glGenTextures(1, &leftInfraredTextureId);
        }
        if (rightInfraredTextureId == 0)
        {
            glGenTextures(1, &rightInfraredTextureId);
        }

        if (config.streaming.sensor.streamLeftInfrared == config.streaming.sensor.streamRightInfrared)
        {
            Log::logv("Upload stereo infrared frame to textures %u %u", (unsigned)leftInfraredTextureId, (unsigned)rightInfraredTextureId);
            // Both true: streaming both eyes
            // Both false: streaming OCC; don't know which eye(s) the frame is from, so assume both
            Gui::cameraFrameToTexture(leftInfraredTextureId, rightInfraredTextureId, samples.infrared.value);
        }
        else if (config.streaming.sensor.streamLeftInfrared)
        {
            Log::logv("Upload left infrared frame to texture %u", (unsigned)leftInfraredTextureId);
            Gui::cameraFrameToTexture(leftInfraredTextureId, samples.infrared.value);
        }
        else if (config.streaming.sensor.streamRightInfrared)
        {
            Log::logv("Upload right infrared frame to texture %u", (unsigned)rightInfraredTextureId);
            Gui::cameraFrameToTexture(rightInfraredTextureId, samples.infrared.value);
        }
    }
}

static const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
static const Gui::GridConfig gridConfig = {
    /* numCellsX */ 2,
    /* numCellsY */ 3,
    /* numTools */ 0,
    /* toolAreaWidth */ 0,
};

/**
 * @brief Render the latest images into the GUI after the textures are loaded
 *
 */
void RecorderGui::Impl::renderSamples()
{
    ImGui::Begin("Depth", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 0, 0);
    if (depthTextureId != 0)
    {
        Gui::drawTextureInContentArea(depthTextureId);
    }
    ImGui::End();

    ImGui::Begin("Visible", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 1, 0);
    if (visibleTextureId != 0)
    {
        Gui::drawTextureInContentArea(visibleTextureId);
    }
    ImGui::End();

    ImGui::Begin("IR Left", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 0, 1);
    if (leftInfraredTextureId != 0)
    {
        Gui::drawTextureInContentArea(leftInfraredTextureId);
    }
    ImGui::End();

    ImGui::Begin("IR Right", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 1, 1);
    if (rightInfraredTextureId != 0)
    {
        Gui::drawTextureInContentArea(rightInfraredTextureId);
    }
    ImGui::End();
}

namespace
{
template <typename T>
bool EnumRadioButton(const char *label, T *value, T ownValue)
{
    int iv = (int)*value;
    bool ret = ImGui::RadioButton(label, &iv, (int)ownValue);
    *value = (T)iv;
    return ret;
}
} // namespace

/**
 * @brief Render the control panel for the Structure Core sensor.
 *
 * @param config - the current configuration options
 */
void RecorderGui::Impl::renderControls(AppConfig &config)
{
    if (config.streaming.source == StreamingSource::Sensor)
    {
        ImGui::Begin("Sensor control", nullptr, windowFlags);
        Gui::layoutWindowAsGridCell(gridConfig, 0, 2);

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

        if (streamAll)
        {
            config.streaming.sensor.streamDepth = true;
            config.streaming.sensor.streamVisible = true;
            config.streaming.sensor.streamLeftInfrared = true;
            config.streaming.sensor.streamRightInfrared = true;
            config.streaming.sensor.streamAccel = true;
            config.streaming.sensor.streamGyro = true;
        }
        else if (streamNone)
        {
            config.streaming.sensor.streamDepth = false;
            config.streaming.sensor.streamVisible = false;
            config.streaming.sensor.streamLeftInfrared = false;
            config.streaming.sensor.streamRightInfrared = false;
            config.streaming.sensor.streamAccel = false;
            config.streaming.sensor.streamGyro = false;
        }
        else if (streamDepthAndVisibleOnly)
        {
            config.streaming.sensor.streamDepth = true;
            config.streaming.sensor.streamVisible = true;
            config.streaming.sensor.streamLeftInfrared = false;
            config.streaming.sensor.streamRightInfrared = false;
            config.streaming.sensor.streamAccel = false;
            config.streaming.sensor.streamGyro = false;
        }
        else if (streamImuOnly)
        {
            config.streaming.sensor.streamDepth = false;
            config.streaming.sensor.streamVisible = false;
            config.streaming.sensor.streamLeftInfrared = false;
            config.streaming.sensor.streamRightInfrared = false;
            config.streaming.sensor.streamAccel = true;
            config.streaming.sensor.streamGyro = true;
        }
        else if (streamDepthAndVisibleWithImu)
        {
            config.streaming.sensor.streamDepth = true;
            config.streaming.sensor.streamVisible = true;
            config.streaming.sensor.streamLeftInfrared = false;
            config.streaming.sensor.streamRightInfrared = false;
            config.streaming.sensor.streamAccel = true;
            config.streaming.sensor.streamGyro = true;
        }
        else if (streamBothInfrared)
        {
            config.streaming.sensor.streamDepth = false;
            config.streaming.sensor.streamVisible = false;
            config.streaming.sensor.streamLeftInfrared = true;
            config.streaming.sensor.streamRightInfrared = true;
            config.streaming.sensor.streamAccel = false;
            config.streaming.sensor.streamGyro = false;
        }

        accelLog.Draw("Accelerometer");
        gyroLog.Draw("Gyroscope");
    }
    else if (config.streaming.source == StreamingSource::OCC)
    {
        ImGui::Begin("OCC control", nullptr, windowFlags);
        Gui::layoutWindowAsGridCell(gridConfig, 1, 2);

        ImGui::Checkbox("Stream OCC", &config.streaming.occ.streamOcc);
        ImGui::Checkbox("Depth correction", &config.depthCorrection);
        ImGui::Checkbox("Frame sync", &config.streaming.frameSync);

        ImGui::End();
    }
}

/**
 * @brief Render summary statistics from the cameras and IMU.
 *
 * @param samples - the latest set of samples from the Structure Core
 */
void RecorderGui::Impl::renderStatistics(const SampleSet &samples)
{
    // buffers for IMU log output
    char gyroBuffer[50];
    char accelBuffer[50];

    ImGui::Begin("Info", nullptr, windowFlags);
    Gui::layoutWindowAsGridCell(gridConfig, 1, 2);
    PlotVar("Depth FPS", samples.depth.rate);
    PlotVar("Visible FPS", samples.visible.rate);
    PlotVar("IR FPS", samples.infrared.rate);
    PlotVar("Accel FPS", samples.accel.rate);
    PlotVar("Gyro FPS", samples.gyro.rate);

    PlotVar("Render FPS", renderMonitor.rate());

    ImGui::Text("Accel: [% .5f % .5f % .5f]", samples.accel.value.acceleration().x, samples.accel.value.acceleration().y, samples.accel.value.acceleration().z);
    ImGui::Text("Gyro:  [% .5f % .5f % .5f]", samples.gyro.value.rotationRate().x, samples.gyro.value.rotationRate().y, samples.gyro.value.rotationRate().z);

    sprintf(accelBuffer, "% .5f % .5f % .5f\r\n", samples.accel.value.acceleration().x, samples.accel.value.acceleration().y, samples.accel.value.acceleration().z);

    if (currentConfig.streaming.sensor.streamAccel)
    {
        accelLog.AddLog(accelBuffer);
    }

    sprintf(gyroBuffer, "% .5f % .5f % .5f\r\n", samples.gyro.value.rotationRate().x, samples.gyro.value.rotationRate().y, samples.gyro.value.rotationRate().z);

    if (currentConfig.streaming.sensor.streamGyro)
    {
        gyroLog.AddLog(gyroBuffer);
    }

    ImGui::End();
}

/**
 * @brief Callback function provided to the render loop utility.
 *
 * @return true - continue rendering new frames
 * @return false - exit the render loop
 */
bool RecorderGui::Impl::renderCallback()
{
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
    setStyles();
    renderSamples();
    renderControls(postRenderConfig);
    renderStatistics(samples);

    bool shouldIssueCallback = false;
    {
        std::unique_lock<std::mutex> u(lock);
        if (requestShutdown)
        {
            // Instruct render loop to exit
            return false;
        }
        if (preRenderConfig != postRenderConfig)
        {
            shouldIssueCallback = !currentConfig.equiv(postRenderConfig);
            currentConfig = postRenderConfig;
            renderLoop->redrawNext();
        }
    }
    if (shouldIssueCallback)
    {
        configCallback(postRenderConfig);
    }
    return true;
}

/**
 * @brief Construct a new Recorder Gui:: Recorder Gui object
 *
 * @param initialConfig - the starting configuration options
 * @param configCallback - callback executed on render
 * @param exitCallback - callback executed on gui exit
 */
RecorderGui::RecorderGui(const AppConfig &initialConfig, ConfigCallback configCallback, ExitCallback exitCallback)
{
    impl = std::make_shared<Impl>();
    impl->currentConfig = initialConfig;
    impl->configCallback = configCallback;
    impl->exitCallback = exitCallback;
    std::thread(Impl::threadMain, impl).detach();
}

/**
 * @brief Destroy the Recorder Gui:: Recorder Gui object
 *
 */
RecorderGui::~RecorderGui()
{
    std::unique_lock<std::mutex> u(impl->lock);
    impl->requestShutdown = true;
    impl->renderLoop->wake();
}

/**
 * @brief Set ImGui styles and colors.
 *
 */
void RecorderGui::Impl::setStyles()
{
    ImGuiStyle *style = &ImGui::GetStyle();

    style->WindowPadding = ImVec2(10.0f, 10.0f);
    style->WindowRounding = 5.0f;
    style->FramePadding = ImVec2(5.0f, 4.0f);
    style->FrameRounding = 5.0f;
    style->ItemSpacing = ImVec2(5.0f, 5.0f);
    style->ItemInnerSpacing = ImVec2(10.0f, 10.0f);
    style->IndentSpacing = 15.0f;
    style->ScrollbarSize = 16.0f;
    style->ScrollbarRounding = 5.0f;
    style->GrabMinSize = 7.0f;
    style->GrabRounding = 2.0f;

    style->Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.59f, 0.59f, 0.59f, 1.00f);
    style->Colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style->Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    style->Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.10f, 1.00f);
    style->Colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.80f);
    style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style->Colors[ImGuiCol_FrameBg] = ImVec4(0.71f, 0.71f, 0.71f, 0.39f);
    style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.00f, 0.59f, 0.80f, 0.43f);
    style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.00f, 0.47f, 0.71f, 0.67f);
    style->Colors[ImGuiCol_TitleBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.80f);
    style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.78f, 0.78f, 0.78f, 0.39f);
    style->Colors[ImGuiCol_TitleBgActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
    style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
    style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.59f);
    style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.78f);
    style->Colors[ImGuiCol_CheckMark] = ImVec4(0.27f, 0.59f, 0.75f, 1.00f);
    style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
    style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.59f);
    style->Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.00f, 0.00f, 0.27f);
    style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.59f, 0.80f, 0.43f);
    style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.47f, 0.71f, 0.67f);
    style->Colors[ImGuiCol_Header] = ImVec4(0.71f, 0.71f, 0.71f, 0.39f);
    style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.51f, 0.67f, 1.00f);
    style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.08f, 0.39f, 0.55f, 1.00f);
    style->Colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.27f, 0.59f, 0.75f, 1.00f);
    style->Colors[ImGuiCol_SeparatorActive] = ImVec4(0.08f, 0.39f, 0.55f, 1.00f);
    style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.78f);
    style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.27f, 0.59f, 0.75f, 0.78f);
    style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.08f, 0.39f, 0.55f, 0.78f);
    style->Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.27f, 0.59f, 0.75f, 1.00f);
    style->Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
}

/**
 * @brief Update the configuration for selected data streams.
 *
 * This method propagates changes to the configuration, updating the GUI
 * to only display the data selected (individual camera streams, accelerometer,
 * gyroscope, etc.).
 *
 * @param config - latest configuration values for the GUI
 */
void RecorderGui::updateDisplayedConfig(const AppConfig &config)
{
    std::unique_lock<std::mutex> u(impl->lock);
    impl->currentConfig = config;
    impl->renderLoop->wake();
}

/**
 * @brief Update the user interface with new data samples.
 *
 * @param samples - data from Structure Core
 */
void RecorderGui::updateSamples(const SampleSet &samples)
{
    std::unique_lock<std::mutex> u(impl->lock);
    impl->currentChanges.registerSamples(impl->currentSamples, samples);
    impl->currentSamples = samples;
    impl->renderLoop->wake();
}

/**
 * @brief Exit the user interface.
 *
 */
void RecorderGui::exit()
{
    std::unique_lock<std::mutex> u(impl->lock);
    impl->requestShutdown = true;
    impl->renderLoop->wake();
}
