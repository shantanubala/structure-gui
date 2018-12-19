#include "Gui.h"

#include <math.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_opengl3.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <ST/CameraFrames.h>

namespace Gui = SampleCode::Gui;

static void handleGlfwError(int err, const char *desc) {
    fprintf(stderr, "GLFW error %d: %s\n", err, desc);
    exit(1);
}

static bool operator==(const ImVec2& a, const ImVec2& b) {
    return a.x == b.x && a.y == b.y;
}

static bool operator!=(const ImVec2& a, const ImVec2& b) {
    return !(a == b);
}

static thread_local Gui::RenderLoop *currentRenderLoop = nullptr;

static void ensureGlfwInitialized() {
    static std::once_flag once;
    std::call_once(once, []() {
        glfwSetErrorCallback(handleGlfwError);
        glfwInit();
    });
}

Gui::RenderLoop::RenderLoop() {
    if (currentRenderLoop) {
        throw std::runtime_error("Cannot initialize multiple render loops on the same thread");
    }
    currentRenderLoop = this;
    ensureGlfwInitialized();
}

Gui::RenderLoop::~RenderLoop() {
    currentRenderLoop = nullptr;
}

void Gui::RenderLoop::main(const std::string& windowTitle, RenderCallback renderCallback) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window = glfwCreateWindow(1024, 768, windowTitle.c_str(), NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glewInit();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowPadding = ImVec2(0, 0);
    ImGui::GetStyle().WindowRounding = 0.f;
    ImVec4 borderColor(0.1f, 0.1f, 0.1f, 1.f);
    ImGui::GetStyle().Colors[ImGuiCol_Border] = borderColor;
    ImGui::GetStyle().Colors[ImGuiCol_TitleBg] = borderColor;
    ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive] = borderColor;

    bool terminateGui = false;
    while (!terminateGui && !glfwWindowShouldClose(window)) {
        if (willRedrawNext) {
            willRedrawNext = false;
            glfwPollEvents();
        }
        else {
            glfwWaitEventsTimeout(1.0);
        }

        glfwMakeContextCurrent(window);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        terminateGui = !renderCallback();

        glfwMakeContextCurrent(window);
        ImGui::Render();
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}

void Gui::RenderLoop::redrawNext() {
    willRedrawNext = true;
}

void Gui::RenderLoop::wake() {
    ensureGlfwInitialized();
    glfwPostEmptyEvent();
}

Gui::RenderLoop& Gui::RenderLoop::current() {
    if (!currentRenderLoop) {
        throw std::runtime_error("No render loop for this thread");
    }
    return *currentRenderLoop;
}

struct GridDimensions {
    unsigned cellAreaX, cellAreaY;
    unsigned cellAreaW, cellAreaH;
    unsigned cellW, cellH;
    unsigned toolAreaX, toolAreaY;
    unsigned toolAreaW, toolAreaH;
    unsigned toolW, toolH;
};

static void computeGridDimensions(const Gui::GridConfig& cfg, GridDimensions& dim) {
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(Gui::RenderLoop::current().window, &fbWidth, &fbHeight);
    unsigned toolAreaWidth = (unsigned)std::max(0, std::min(fbWidth, (int)cfg.toolAreaWidth));
    unsigned cellAreaWidth = (unsigned)std::max(0, fbWidth - (int)toolAreaWidth);
    dim.cellAreaX = 0;
    dim.cellAreaY = 0;
    dim.cellAreaW = cellAreaWidth;
    dim.cellAreaH = fbHeight;
    dim.cellW = cellAreaWidth / std::max(1u, cfg.numCellsX);
    dim.cellH = fbHeight / std::max(1u, cfg.numCellsY);
    dim.toolAreaX = cellAreaWidth;
    dim.toolAreaY = 0;
    dim.toolAreaW = toolAreaWidth;
    dim.toolAreaH = fbHeight;
    dim.toolW = toolAreaWidth;
    dim.toolH = fbHeight / std::max(1u, cfg.numTools);
}

void Gui::layoutWindowAsGridCell(const GridConfig& cfg, unsigned cellX, unsigned cellY) {
    if (cellX >= cfg.numCellsX || cellY >= cfg.numCellsY) {
        throw std::runtime_error("Cell position out of range for config");
    }
    GridDimensions dim;
    computeGridDimensions(cfg, dim);
    ImVec2 newPos(float(dim.cellAreaX + cellX * dim.cellW), float(dim.cellAreaY + cellY * dim.cellH));
    ImVec2 newSize(float(dim.cellW), float(dim.cellH));
    if (ImGui::GetWindowPos() != newPos) {
        ImGui::SetWindowPos(newPos);
    }
    if (ImGui::GetWindowSize() != newSize) {
        ImGui::SetWindowSize(newSize);
    }
}


void Gui::layoutWindowAsGridTool(const GridConfig& cfg, unsigned toolN) {
    if (toolN >= cfg.numTools) {
        throw std::runtime_error("Tool number out of range for config");
    }
    GridDimensions dim;
    computeGridDimensions(cfg, dim);
    ImVec2 newPos(float(dim.toolAreaX), float(dim.toolAreaY + toolN * dim.toolH));
    ImVec2 newSize(ImVec2(float(dim.toolW), float(dim.toolH)));
    if (ImGui::GetWindowPos() != newPos) {
        ImGui::SetWindowPos(newPos);
    }
    if (ImGui::GetWindowSize() != newSize) {
        ImGui::SetWindowSize(newSize);
    }
}

void Gui::drawTextureInContentArea(uint32_t textureId) {
    GLint iwidth = 0, iheight = 0;
    glBindTexture(GL_TEXTURE_2D, textureId);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iwidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &iheight);
    glBindTexture(GL_TEXTURE_2D, 0);
    float width = iwidth;
    float height = iheight;
    float contentWidth = ImGui::GetContentRegionAvail().x;
    float contentHeight = ImGui::GetContentRegionAvail().y;
    float fitWidth = 0.f;
    float fitHeight = 0.f;
    float xOffset = 0.f;
    float yOffset = 0.f;
    if (width >= 1.f && height >= 1.f && contentWidth >= 1.f && contentHeight >= 1.f) {
        float minFit = std::max(width / contentWidth, height / contentHeight);
        fitWidth = width / minFit;
        fitHeight = height / minFit;
        xOffset = (contentWidth - fitWidth) / 2;
        yOffset = (contentHeight - fitHeight) / 2;
    }
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
    ImGui::Image((void *)(uintptr_t)textureId, ImVec2(fitWidth, fitHeight));
}

void Gui::cameraFrameToTexture(uint32_t textureId, const ST::DepthFrame& frame) {
    int npx = frame.width() * frame.height();
    auto rgb = new uint8_t[3 * npx];
    static const float minDepth = 700.f;
    static const float maxDepth = 5000.f;
    for (int i = 0; i < npx; ++i) {
        float d = frame.depthInMillimeters()[i];
        if (std::isnan(d)) {
            rgb[3 * i + 0] = 0;
            rgb[3 * i + 1] = 0;
            rgb[3 * i + 2] = 0;
        }
        else {
            uint32_t pval = (uint32_t)std::max(0, std::min(6 * 0xff, int32_t(6 * 0xff * (d - minDepth) / (maxDepth - minDepth))));
            uint8_t lb = pval & 0xff;
            uint8_t *px = &rgb[3 * i];
            switch (pval >> 8) {
                case 0: px[0] = 0xff; px[1] = 0xff - lb; px[2] = 0xff - lb; break;
                case 1: px[0] = 0xff; px[1] = lb; px[2] = 0; break;
                case 2: px[0] = 0xff - lb; px[1] = 0xff; px[2] = 0; break;
                case 3: px[0] = 0; px[1] = 0xff; px[2] = lb; break;
                case 4: px[0] = 0; px[1] = 0xff - lb; px[2] = 0xff; break;
                case 5: px[0] = 0; px[1] = 0; px[2] = 0xff - lb; break;
                default: px[0] = 0xff; px[1] = 0xff; px[2] = 0xff; break;
            }
        }
    }
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame.width(), frame.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glBindTexture(GL_TEXTURE_2D, 0);
    delete[] rgb;
}

void Gui::cameraFrameToTexture(uint32_t textureId, const ST::ColorFrame& frame) {
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame.width(), frame.height(), 0, GL_RGB, GL_UNSIGNED_BYTE, frame.rgbData());
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void irDataToTexture(uint32_t textureId, const uint16_t *data, int width, int height, int xOffset, int rowStride) {
    static uint8_t lut[0x10000];
    static std::once_flag once;
    std::call_once(once, [&]() {
        for (size_t i = 0; i < sizeof lut; ++i) {
            // 10-bit resolution; clamp out-of-range
            lut[i] = uint8_t((uint32_t)std::min(i, (size_t)0x3ff) * 0xffu / 0x3ffu);
        }
    });
    auto rgb = new uint8_t[3 * width * height];
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t conv = lut[data[y * rowStride + xOffset + x]];
            rgb[3 * (y * width + x) + 0] = conv;
            rgb[3 * (y * width + x) + 1] = conv;
            rgb[3 * (y * width + x) + 2] = conv;
        }
    }
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
    glBindTexture(GL_TEXTURE_2D, 0);
    delete[] rgb;
}

void Gui::cameraFrameToTexture(uint32_t textureId, const ST::InfraredFrame& frame) {
    irDataToTexture(textureId, frame.data(), frame.width(), frame.height(), 0, frame.width());
}

void Gui::cameraFrameToTexture(uint32_t textureIdLeft, uint32_t textureIdRight, const ST::InfraredFrame& frame) {
    int eyeWidth = frame.width() / 2;
    irDataToTexture(textureIdLeft, frame.data(), eyeWidth, frame.height(), eyeWidth, frame.width());
    irDataToTexture(textureIdRight, frame.data(), eyeWidth, frame.height(), 0, frame.width());
}
