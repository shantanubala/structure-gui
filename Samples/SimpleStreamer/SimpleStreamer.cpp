#include <condition_variable>
#include <mutex>
#include <stdio.h>

#include <ST/CaptureSession.h>

struct SessionDelegate : ST::CaptureSessionDelegate {
    std::mutex lock;
    std::condition_variable cond;
    bool ready = false;
    bool done = false;

    void captureSessionEventDidOccur(ST::CaptureSession *, ST::CaptureSessionEventId event) override {
        printf("Received capture session event %d (%s)\n", (int)event, ST::CaptureSessionSample::toString(event));
        switch (event) {
            case ST::CaptureSessionEventId::Ready: {
                std::unique_lock<std::mutex> u(lock);
                ready = true;
                cond.notify_all();
            } break;
            case ST::CaptureSessionEventId::Disconnected:
            case ST::CaptureSessionEventId::EndOfFile:
            case ST::CaptureSessionEventId::Error: {
                std::unique_lock<std::mutex> u(lock);
                done = true;
                cond.notify_all();
            } break;
            default:
                printf("Event %d unhandled\n", (int)event);
        }
    }

    void captureSessionDidOutputSample(ST::CaptureSession *, const ST::CaptureSessionSample& sample) {
        printf("Received capture session sample of type %d (%s)\n", (int)sample.type, ST::CaptureSessionSample::toString(sample.type));
        switch (sample.type) {
            case ST::CaptureSessionSample::Type::DepthFrame:
                printf("Depth frame: size %dx%d\n", sample.depthFrame.width(), sample.depthFrame.height());
                break;
            case ST::CaptureSessionSample::Type::VisibleFrame:
                printf("Visible frame: size %dx%d\n", sample.visibleFrame.width(), sample.visibleFrame.height());
                break;
            case ST::CaptureSessionSample::Type::InfraredFrame:
                printf("Infrared frame: size %dx%d\n", sample.infraredFrame.width(), sample.infraredFrame.height());
                break;
            case ST::CaptureSessionSample::Type::SynchronizedFrames:
                printf("Synchronized frames: depth %dx%d visible %dx%d infrared %dx%d\n", sample.depthFrame.width(), sample.depthFrame.height(), sample.visibleFrame.width(), sample.visibleFrame.height(), sample.infraredFrame.width(), sample.infraredFrame.height());
                break;
            case ST::CaptureSessionSample::Type::AccelerometerEvent:
                printf("Accelerometer event: [% .5f % .5f % .5f]\n", sample.accelerometerEvent.acceleration().x, sample.accelerometerEvent.acceleration().y, sample.accelerometerEvent.acceleration().z);
                break;
            case ST::CaptureSessionSample::Type::GyroscopeEvent:
                printf("Gyroscope event: [% .5f % .5f % .5f]\n", sample.gyroscopeEvent.rotationRate().x, sample.gyroscopeEvent.rotationRate().y, sample.gyroscopeEvent.rotationRate().z);
                break;
            default:
                printf("Sample type %d unhandled\n", (int)sample.type);
        }
    }

    void waitUntilReady() {
        std::unique_lock<std::mutex> u(lock);
        cond.wait(u, [this]() {
            return ready;
        });
    }

    void waitUntilDone() {
        std::unique_lock<std::mutex> u(lock);
        cond.wait(u, [this]() {
            return done;
        });
    }
};

int main(void) {
    ST::CaptureSessionSettings settings;
    settings.source = ST::CaptureSessionSourceId::StructureCore;
    settings.structureCore.depthEnabled = true;
    settings.structureCore.visibleEnabled = true;
    settings.structureCore.infraredEnabled = true;
    settings.structureCore.accelerometerEnabled = true;
    settings.structureCore.gyroscopeEnabled = true;
    settings.structureCore.depthResolution = ST::StructureCoreDepthResolution::VGA;
    settings.structureCore.imuUpdateRate = ST::StructureCoreIMUUpdateRate::AccelAndGyro_200Hz;

    SessionDelegate delegate;
    ST::CaptureSession session;
    session.setDelegate(&delegate);
    if (!session.startMonitoring(settings)) {
        printf("Failed to initialize capture session\n");
        return 1;
    }

    printf("Waiting for session to become ready...\n");
    delegate.waitUntilReady();
    session.startStreaming();
    delegate.waitUntilDone();
    session.stopStreaming();
    return 0;
}
