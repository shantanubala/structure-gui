#include "Playground.h"
#include <SampleCode/SampleCode.h>
#include <ST/CameraFrames.h>
#include <ST/CaptureSession.h>
#include <ST/OCCFileWriter.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string.h>

namespace Gui = SampleCode::Gui;
namespace Log = SampleCode::Log;

static const char usageMsg[] =
    "usage: Recorder [-h] [-v|-q] [streaming options...]\n"
    "-h/--help: Show this message\n"
    "-v/--verbose: Emit additional log messages\n"
    "-q/--quiet: Emit no log messages\n"
    "-H/--headless: Run without graphical interface\n"
    "-D/--depth: Stream depth data at startup\n"
    "-V/--visible: Stream visible camera at startup\n"
    "-I/--infrared-both: Stream both infrared cameras at startup\n"
    "-L/--infrared-left: Stream left infrared camera at startup\n"
    "-R/--infrared-right: Stream right infrared camera at startup\n"
    "-A/--accelerometer: Stream accelerometer at startup\n"
    "-G/--gyroscope: Stream gyroscope at startup\n"
    "-O/--occ: Stream OCC at startup (if --input-occ given)\n"
    "-d/--depth-correction: Enable temperature compensation and speckle filter\n"
    "-f/--fast-playback: Play back OCC as fast as possible without dropping frames (non-realtime)\n"
    "-i/--input-occ <file>: Play back OCC from <file>\n"
    "-o/--output-occ <file>: Record OCC to <file>\n"
    "-s/--serial <serial>: Use sensor with given serial number (default: first found)\n"
    "-t/--time <milliseconds>: How long to stream from device or OCC; no limit if negative (default)\n"
    "-x/--exit-on-end: Exit at end of OCC or --time duration\n"
    "--no-frame-sync: Do not synchronize frames from device or OCC\n"
    "";

static void parseOptions(AppConfig &config, int argc, char **argv)
{
#define NEXT                                                               \
    do                                                                     \
    {                                                                      \
        if (++i >= argc)                                                   \
        {                                                                  \
            fprintf(stderr, "Expected argument after: %s\n", argv[i - 1]); \
            fputs(usageMsg, stderr);                                       \
            exit(1);                                                       \
        }                                                                  \
    } while (0)
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            fputs(usageMsg, stdout);
            exit(0);
        }
        else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
        {
            Log::setLogLevel(Log::Level::Verbose);
        }
        else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet"))
        {
            Log::setLogLevel(Log::Level::Quiet);
        }
        else if (!strcmp(argv[i], "-H") || !strcmp(argv[i], "--headless"))
        {
            config.headless = true;
        }
        else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--depth"))
        {
            config.streaming.sensor.streamDepth = true;
        }
        else if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--visible"))
        {
            config.streaming.sensor.streamVisible = true;
        }
        else if (!strcmp(argv[i], "-I") || !strcmp(argv[i], "--infrared-both"))
        {
            config.streaming.sensor.streamLeftInfrared = true;
            config.streaming.sensor.streamRightInfrared = true;
        }
        else if (!strcmp(argv[i], "-L") || !strcmp(argv[i], "--infrared-left"))
        {
            config.streaming.sensor.streamLeftInfrared = true;
        }
        else if (!strcmp(argv[i], "-R") || !strcmp(argv[i], "--infrared-right"))
        {
            config.streaming.sensor.streamRightInfrared = true;
        }
        else if (!strcmp(argv[i], "-A") || !strcmp(argv[i], "--accelerometer"))
        {
            config.streaming.sensor.streamAccel = true;
        }
        else if (!strcmp(argv[i], "-G") || !strcmp(argv[i], "--gyroscope"))
        {
            config.streaming.sensor.streamGyro = true;
        }
        else if (!strcmp(argv[i], "-O") || !strcmp(argv[i], "--occ"))
        {
            config.streaming.occ.streamOcc = true;
        }
        else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--depth-correction"))
        {
            config.depthCorrection = true;
        }
        else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--fast-playback"))
        {
            config.streaming.occ.fastPlayback = true;
        }
        else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--input-occ"))
        {
            NEXT;
            config.streaming.source = StreamingSource::OCC;
            config.inputOccPath = argv[i];
        }
        else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output-occ"))
        {
            NEXT;
            config.outputOccPath = argv[i];
        }
        else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--serial"))
        {
            NEXT;
            config.streaming.sensor.serial = argv[i];
        }
        else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--time"))
        {
            NEXT;
            config.streamDuration = std::stoi(argv[i]);
        }
        else if (!strcmp(argv[i], "-x") || !strcmp(argv[i], "--exit-on-end"))
        {
            config.exitOnEnd = true;
        }
        else if (!strcmp(argv[i], "--no-frame-sync"))
        {
            config.streaming.frameSync = false;
        }
        else
        {
            fprintf(stderr, "Argument not understood: %s\n", argv[i]);
            fputs(usageMsg, stderr);
            exit(1);
        }
    }
#undef NEXT
}

static ST::CaptureSessionSettings sessionSettingsForConfig(const AppConfig &config)
{
    ST::CaptureSessionSettings settings;
    settings.frameSyncEnabled = config.streaming.frameSync;

    if (config.streaming.source == StreamingSource::Sensor)
    {
        settings.source = ST::CaptureSessionSourceId::StructureCore;
        settings.structureCore.sensorSerial = config.streaming.sensor.serial.empty() ? nullptr : config.streaming.sensor.serial.c_str(); // startMonitoring() copies string
        settings.structureCore.depthEnabled = config.streaming.sensor.streamDepth;
        settings.structureCore.visibleEnabled = config.streaming.sensor.streamVisible;
        if (config.streaming.sensor.streamLeftInfrared && config.streaming.sensor.streamRightInfrared)
        {
            settings.structureCore.infraredEnabled = true;
            settings.structureCore.infraredMode = ST::StructureCoreInfraredMode::BothCameras;
        }
        else if (config.streaming.sensor.streamLeftInfrared)
        {
            settings.structureCore.infraredEnabled = true;
            settings.structureCore.infraredMode = ST::StructureCoreInfraredMode::LeftCameraOnly;
        }
        else if (config.streaming.sensor.streamRightInfrared)
        {
            settings.structureCore.infraredEnabled = true;
            settings.structureCore.infraredMode = ST::StructureCoreInfraredMode::RightCameraOnly;
        }
        else
        {
            settings.structureCore.infraredEnabled = false;
        }
        settings.structureCore.accelerometerEnabled = config.streaming.sensor.streamAccel;
        settings.structureCore.gyroscopeEnabled = config.streaming.sensor.streamGyro;
        switch (config.streaming.sensor.depthResolution)
        {
        case DepthResolution::VGA:
            settings.structureCore.depthResolution = ST::StructureCoreDepthResolution::VGA;
            break;
        case DepthResolution::Full:
            settings.structureCore.depthResolution = ST::StructureCoreDepthResolution::SXGA;
            break;
        default:;
        }
    }

    else if (config.streaming.source == StreamingSource::OCC)
    {
        settings.source = ST::CaptureSessionSourceId::OCC;
        settings.occ.path = config.inputOccPath.c_str(); // startMonitoring() copies string
        settings.occ.autoReplay = false;
        if (config.streaming.occ.fastPlayback)
        {
            settings.occ.playbackMode = ST::CaptureSessionOCCPlaybackMode::NonDropping;
        }
        else
        {
            settings.occ.playbackMode = ST::CaptureSessionOCCPlaybackMode::RealTime;
        }
    }

    return settings;
}

namespace
{
struct SessionContext
{
    std::unique_ptr<RecorderGui> gui;

    std::mutex lock;
    std::condition_variable cond;
    AppConfig config;
    SampleSet samples;
    bool readyToStream = false;
    bool endOfStream = false;
    bool streamError = false;

    std::chrono::steady_clock::duration accumulatedDuration;
    bool haveFirstSample = false;
    std::chrono::steady_clock::time_point lastSampleTime;

    std::mutex occWriterLock;
    std::unique_ptr<ST::OCCFileWriter> occWriter;

    RateMonitor depthMonitor;
    RateMonitor visibleMonitor;
    RateMonitor infraredMonitor;
    RateMonitor accelMonitor;
    RateMonitor gyroMonitor;

    void reset()
    {
        readyToStream = false;
        endOfStream = false;
        streamError = false;
        accumulatedDuration = std::chrono::seconds(0);
        haveFirstSample = false;
    }
};
}; // namespace

static void handleSessionEvent(SessionContext &ctx, ST::CaptureSessionEventId event)
{
    Log::log("Session event %d: %s", (int)event, ST::CaptureSessionSample::toString(event));
    std::unique_lock<std::mutex> u(ctx.lock);
    switch (event)
    {
    case ST::CaptureSessionEventId::Ready:
        ctx.readyToStream = true;
        break;
    case ST::CaptureSessionEventId::Disconnected:
        ctx.endOfStream = true;
        break;
    case ST::CaptureSessionEventId::EndOfFile:
        ctx.endOfStream = true;
        break;
    case ST::CaptureSessionEventId::Error:
        ctx.streamError = true;
        break;
    default:
        Log::log("Not handling session event %d: %s", (int)event, ST::CaptureSessionSample::toString(event));
    }
    ctx.cond.notify_all();
}

static void handleSessionOutput(SessionContext &ctx, const ST::CaptureSessionSample &sample)
{
    Log::logv("New sample of type %d: %s", (int)sample.type, ST::CaptureSessionSample::toString(sample.type));

    bool depthCorrectionEnabled = false;
    {
        std::unique_lock<std::mutex> u(ctx.lock);
        depthCorrectionEnabled = ctx.config.depthCorrection;
    }
    // Slow, do outside context lock
    if (depthCorrectionEnabled && sample.depthFrame.isValid())
    {
        // Internals of const ST::DepthFrame are still mutable
        ST::DepthFrame x = sample.depthFrame;
        x.applyExpensiveCorrection();
    }

    ctx.occWriterLock.lock();
    if (ctx.occWriter)
    {
        ctx.occWriter->writeCaptureSample(sample);
    }
    ctx.occWriterLock.unlock();

    SampleSet samples;
    {
        std::unique_lock<std::mutex> u(ctx.lock);
        switch (sample.type)
        {
        case ST::CaptureSessionSample::Type::DepthFrame:
        {
            ctx.depthMonitor.tick();
            ctx.samples.depth.newSample(sample.depthFrame, ctx.depthMonitor.rate());
        }
        break;
        case ST::CaptureSessionSample::Type::VisibleFrame:
        {
            ctx.visibleMonitor.tick();
            ctx.samples.visible.newSample(sample.visibleFrame, ctx.visibleMonitor.rate());
        }
        break;
        case ST::CaptureSessionSample::Type::InfraredFrame:
        {
            ctx.infraredMonitor.tick();
            ctx.samples.infrared.newSample(sample.infraredFrame, ctx.infraredMonitor.rate());
        }
        break;
        case ST::CaptureSessionSample::Type::AccelerometerEvent:
        {
            ctx.accelMonitor.tick();
            ctx.samples.accel.newSample(sample.accelerometerEvent, ctx.accelMonitor.rate());
        }
        break;
        case ST::CaptureSessionSample::Type::GyroscopeEvent:
        {
            ctx.gyroMonitor.tick();
            ctx.samples.gyro.newSample(sample.gyroscopeEvent, ctx.gyroMonitor.rate());
        }
        break;
        case ST::CaptureSessionSample::Type::SynchronizedFrames:
        {
            if (sample.depthFrame.isValid())
            {
                ctx.depthMonitor.tick();
                ctx.samples.depth.newSample(sample.depthFrame, ctx.depthMonitor.rate());
            }
            if (sample.visibleFrame.isValid())
            {
                ctx.visibleMonitor.tick();
                ctx.samples.visible.newSample(sample.visibleFrame, ctx.visibleMonitor.rate());
            }
            if (sample.infraredFrame.isValid())
            {
                ctx.infraredMonitor.tick();
                ctx.samples.infrared.newSample(sample.infraredFrame, ctx.infraredMonitor.rate());
            }
        }
        break;
        default:
            Log::logv("Not handling sample of type %d: %s", (int)sample.type, ST::CaptureSessionSample::toString(sample.type));
        }
        if (ctx.config.streamDuration >= 0)
        {
            auto now = std::chrono::steady_clock::now();
            if (ctx.haveFirstSample)
            {
                ctx.accumulatedDuration += now - ctx.lastSampleTime;
                auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(ctx.accumulatedDuration).count();
                if ((long long)msec >= (long long)ctx.config.streamDuration && !ctx.endOfStream)
                {
                    Log::log("Duration given by --time elapsed, ending stream");
                    ctx.endOfStream = true;
                    ctx.cond.notify_all();
                }
            }
            ctx.haveFirstSample = true;
            ctx.lastSampleTime = now;
        }
        samples = ctx.samples;
    }
    if (ctx.gui)
    {
        ctx.gui->updateSamples(samples);
    }
}

struct SessionDelegate : ST::CaptureSessionDelegate
{
    SessionContext &ctx;
    SessionDelegate(SessionContext &ctx_) : ctx(ctx_) {}
    void captureSessionEventDidOccur(ST::CaptureSession *, ST::CaptureSessionEventId event) override
    {
        handleSessionEvent(ctx, event);
    }
    void captureSessionDidOutputSample(ST::CaptureSession *, const ST::CaptureSessionSample &sample) override
    {
        handleSessionOutput(ctx, sample);
    }
};

static int sessionControlLoop(const AppConfig &initialConfig)
{
    Log::log("Enter session control loop");
    bool exitApp = false;
    int exitStatus = 0;

    SessionContext ctx;
    ctx.config = initialConfig;
    if (!ctx.config.headless)
    {
        auto guiConfigCallback = [&ctx](const AppConfig &newConfig) {
            std::unique_lock<std::mutex> u(ctx.lock);
            ctx.config = newConfig;
            ctx.cond.notify_all();
        };
        auto guiExitCallback = [&ctx, &exitApp]() {
            std::unique_lock<std::mutex> u(ctx.lock);
            exitApp = true;
            ctx.cond.notify_all();
        };
        Log::log("Start GUI");
        ctx.gui = std::make_unique<RecorderGui>(ctx.config, guiConfigCallback, guiExitCallback);
    }

    bool waitForConfigChange = false;
    AppConfig runningConfig;
    while (true)
    {
        if (waitForConfigChange)
        {
            std::unique_lock<std::mutex> u(ctx.lock);
            while (
                ctx.config.streaming.equiv(runningConfig.streaming) &&
                !exitApp)
            {
                ctx.cond.wait(u);
            }
            waitForConfigChange = false;
        }

        {
            std::unique_lock<std::mutex> u(ctx.lock);
            while (
                !ctx.config.streaming.anyStreamsEnabled() &&
                !exitApp)
            {
                ctx.cond.wait(u);
            }
            if (exitApp)
            {
                Log::log("App exit requested, leaving session control loop");
                break;
            }
            runningConfig = ctx.config;
            ctx.reset();
        }

        ST::CaptureSession session;
        SessionDelegate delegate(ctx);
        session.setDelegate(&delegate);
        session.startMonitoring(sessionSettingsForConfig(runningConfig));

        // OCC input does not generate CaptureSessionEventId::Ready
        if (runningConfig.streaming.source == StreamingSource::Sensor)
        {
            Log::log("Waiting for session to become ready...");
            std::unique_lock<std::mutex> u(ctx.lock);
            while (
                ctx.config.streaming.equiv(runningConfig.streaming) &&
                !ctx.readyToStream &&
                !ctx.streamError &&
                !exitApp)
            {
                ctx.cond.wait(u);
            }
            if (!ctx.config.streaming.equiv(runningConfig.streaming))
            {
                Log::log("Config changed during session setup");
                // Config changed, restart with new config
                continue;
            }
            else if (ctx.readyToStream)
            {
                Log::log("Session ready to stream");
                // Session OK, continue
            }
            else if (ctx.streamError)
            {
                Log::log("Error occurred during session setup");
                // Error occurred, don't try to start streaming; wait for user action before restarting
                waitForConfigChange = true;
                if (ctx.config.exitOnEnd)
                {
                    Log::log("Exiting after session error because --exit-on-end given");
                    exitApp = true;
                    exitStatus = 1;
                }
                continue;
            }
            else if (exitApp)
            {
                Log::log("App exit requested during session setup");
                continue;
            }
        }

        ctx.occWriterLock.lock();
        if (!runningConfig.outputOccPath.empty())
        {
            Log::log("Create OCC writer for path %s", runningConfig.outputOccPath.c_str());
            ctx.occWriter = std::make_unique<ST::OCCFileWriter>();
            ctx.occWriter->startWritingToFile(runningConfig.outputOccPath.c_str());
        }
        else
        {
            ctx.occWriter = nullptr;
        }
        ctx.occWriterLock.unlock();
        Log::log("Start streaming");
        session.startStreaming();
        // Samples now arriving...

        {
            // Wait for end of capture
            std::unique_lock<std::mutex> u(ctx.lock);
            while (
                ctx.config.streaming.equiv(runningConfig.streaming) &&
                !ctx.endOfStream &&
                !ctx.streamError &&
                !exitApp)
            {
                ctx.cond.wait(u);
            }
            if (!ctx.config.streaming.equiv(runningConfig.streaming))
            {
                Log::log("Config changed during streaming");
                // Config changed, restart with new config
            }
            else if (ctx.endOfStream)
            {
                Log::log("End of stream");
                // Stream ended, wait for user action before restarting
                waitForConfigChange = true;
                if (ctx.config.exitOnEnd)
                {
                    Log::log("Exiting after end of stream because --exit-on-end given");
                    exitApp = true;
                }
            }
            else if (ctx.streamError)
            {
                // Error occurred, wait for user action before restarting
                waitForConfigChange = true;
                if (ctx.config.exitOnEnd)
                {
                    Log::log("Exiting after session error because --exit-on-end given");
                    exitApp = true;
                    exitStatus = 1;
                }
            }
            else if (exitApp)
            {
                Log::log("App exit requested during streaming");
                // Do cleanup then exit
            }
        }
        session.stopStreaming();
        ctx.occWriterLock.lock();
        if (ctx.occWriter)
        {
            Log::log("Finalize OCC writer");
            ctx.occWriter->finalizeWriting();
            ctx.occWriter = nullptr;
        }
        ctx.occWriterLock.unlock();
    }

    if (ctx.gui)
    {
        Log::log("Terminate GUI");
        ctx.gui->exit();
    }
    Log::log("Session control loop exiting with status %d", exitStatus);
    return exitStatus;
}

int main(int argc, char **argv)
{
    AppConfig config;
    parseOptions(config, argc, argv);
    if (config.headless && !config.streaming.anyStreamsEnabled())
    {
        fputs("Headless mode enabled but no streams enabled. This will not do anything useful.\n", stderr);
        return 1;
    }
    return sessionControlLoop(config);
}
