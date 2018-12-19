#pragma once

#include "Gui.h"
#include <SampleCode/Log.h>
#include <ST/CameraFrames.h>
#include <ST/IMUEvents.h>

#include <chrono>
#include <cmath>

enum class StreamingSource {
    Sensor,
    OCC,
};

enum class DepthResolution {
    VGA,
    Full,
};

struct SensorConfig {
    std::string serial;
    bool streamDepth = false;
    bool streamVisible = false;
    bool streamLeftInfrared = false;
    bool streamRightInfrared = false;
    bool streamAccel = false;
    bool streamGyro = false;
    DepthResolution depthResolution = DepthResolution::VGA;

    bool equiv(const SensorConfig& x) const {
        return (
            serial == x.serial &&
            streamDepth == x.streamDepth &&
            streamVisible == x.streamVisible &&
            streamLeftInfrared == x.streamLeftInfrared &&
            streamRightInfrared == x.streamRightInfrared &&
            streamAccel == x.streamAccel &&
            streamGyro == x.streamGyro &&
            /* Only compared if relevant */ (streamDepth ? depthResolution == x.depthResolution : true) &&
            true
        );
    }
    bool operator==(const SensorConfig& x) const {
        return (
            serial == x.serial &&
            streamDepth == x.streamDepth &&
            streamVisible == x.streamVisible &&
            streamLeftInfrared == x.streamLeftInfrared &&
            streamRightInfrared == x.streamRightInfrared &&
            streamAccel == x.streamAccel &&
            streamGyro == x.streamGyro &&
            depthResolution == x.depthResolution &&
            true
        );
    }
    bool operator!=(const SensorConfig& x) const {
        return !(*this == x);
    }
    bool anyStreamsEnabled() const {
        return (
            streamDepth ||
            streamVisible ||
            streamLeftInfrared ||
            streamRightInfrared ||
            streamAccel ||
            streamGyro ||
            false
        );
    }
};

struct OccConfig {
    bool streamOcc = false;
    bool fastPlayback = false;

    bool equiv(const OccConfig& x) const {
        return (
            streamOcc == x.streamOcc &&
            fastPlayback == x.fastPlayback &&
            true
        );
    }
    bool operator==(const OccConfig& x) const {
        return (
            streamOcc == x.streamOcc &&
            fastPlayback == x.fastPlayback &&
            true
        );
    }
    bool operator!=(const OccConfig& x) const {
        return !(*this == x);
    }
    bool anyStreamsEnabled() const {
        return (
            streamOcc ||
            false
        );
    }
};

struct StreamingConfig {
    bool frameSync = true;
    StreamingSource source = StreamingSource::Sensor;
    SensorConfig sensor;
    OccConfig occ;

    bool equiv(const StreamingConfig& x) const {
        return (
            frameSync == x.frameSync &&
            source == x.source &&
            /* Only compared if relevant */ (source == StreamingSource::Sensor ? sensor.equiv(x.sensor) : true) &&
            /* Only compared if relevant */ (source == StreamingSource::OCC ? occ.equiv(x.occ) : true) &&
            true
        );
    }
    bool operator==(const StreamingConfig& x) const {
        return (
            frameSync == x.frameSync &&
            source == x.source &&
            sensor == x.sensor &&
            occ == x.occ &&
            true
        );
    }
    bool operator!=(const StreamingConfig& x) const {
        return !(*this == x);
    }
    bool anyStreamsEnabled() const {
        return (
            (source == StreamingSource::Sensor && sensor.anyStreamsEnabled()) ||
            (source == StreamingSource::OCC && occ.anyStreamsEnabled()) ||
            false
        );
    }
};

struct AppConfig {
    // Startup settings
    bool headless = false;
    bool exitOnEnd = false;
    int streamDuration = -1;
    std::string inputOccPath = "";
    std::string outputOccPath = "";
    // Can change on the fly
    bool depthCorrection = false;
    // Changes to streaming config require reconfiguring capture session
    StreamingConfig streaming;

    bool equiv(const AppConfig& x) const {
        return (
            headless == x.headless &&
            exitOnEnd == x.exitOnEnd &&
            streamDuration == x.streamDuration &&
            inputOccPath == x.inputOccPath &&
            outputOccPath == x.outputOccPath &&
            depthCorrection == x.depthCorrection &&
            streaming.equiv(x.streaming) &&
            true
        );
    }
    bool operator==(const AppConfig& x) const {
        return (
            headless == x.headless &&
            exitOnEnd == x.exitOnEnd &&
            streamDuration == x.streamDuration &&
            inputOccPath == x.inputOccPath &&
            outputOccPath == x.outputOccPath &&
            depthCorrection == x.depthCorrection &&
            streaming == x.streaming &&
            true
        );
    }
    bool operator!=(const AppConfig& x) const {
        return !(*this == x);
    }
};

struct SampleSet {
    template<typename T> struct Sample {
        T value;
        double rate = NAN;
        unsigned generation = 0;

        void newSample(const T& x, double rate_) {
            value = x;
            rate = rate_;
            ++generation;
        }
    };
    Sample<ST::DepthFrame> depth;
    Sample<ST::ColorFrame> visible;
    Sample<ST::InfraredFrame> infrared;
    Sample<ST::AccelerometerEvent> accel;
    Sample<ST::GyroscopeEvent> gyro;
};

struct SampleChanges {
    bool depth = false;
    bool visible = false;
    bool infrared = false;
    bool accel = false;
    bool gyro = false;

    void registerSamples(const SampleSet& a, const SampleSet& b) {
        depth = depth || a.depth.generation != b.depth.generation;
        visible = visible || a.visible.generation != b.visible.generation;
        infrared = infrared || a.infrared.generation != b.infrared.generation;
        accel = accel || a.accel.generation != b.accel.generation;
        gyro = gyro || a.gyro.generation != b.gyro.generation;
    }
};

class RateMonitor {
    const double alpha = 0.25;
    const double epsilon = 0.0001;
    bool haveLastTime = false;
    std::chrono::steady_clock::time_point lastTime;
    double period = 0.0;

public:
    double rate() {
        return std::fabs(period) >= epsilon ? 1 / period : NAN;
    }
    void tick() {
        auto now = std::chrono::steady_clock::now();
        if (haveLastTime) {
            double dt = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime).count() / 1.0e9;
            period = alpha * dt + (1 - alpha) * period;
        }
        lastTime = now;
        haveLastTime = true;
    }
    void reset() {
        haveLastTime = false;
        period = 0.0;
    }
};
