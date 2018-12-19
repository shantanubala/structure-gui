#pragma once

#include <functional>
#include <memory>

struct AppConfig;
struct SampleSet;

class RecorderGui {
private:
    struct Impl;
    std::shared_ptr<Impl> impl;

public:
    using ConfigCallback = std::function<void (const AppConfig&)>;
    using ExitCallback = std::function<void ()>;

    /** initialConfig: Initial streaming configuration to show in GUI.
        configCallback: Called in render loop when user changes config in GUI.
        exitCallback: Called in render loop when GUI exits due to exit() or user action. */
    RecorderGui(const AppConfig& initialConfig, ConfigCallback configCallback, ExitCallback exitCallback);
    ~RecorderGui();

    /** Update config displayed in GUI */
    void updateDisplayedConfig(const AppConfig&);
    /** Update samples displayed in GUI */
    void updateSamples(const SampleSet&);
    /** Instruct GUI to exit */
    void exit();

    RecorderGui(const RecorderGui&) = delete;
    RecorderGui(RecorderGui&&) = delete;
    RecorderGui& operator=(const RecorderGui&) = delete;
    RecorderGui& operator=(RecorderGui&&) = delete;
};
