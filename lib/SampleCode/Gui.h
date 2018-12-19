#pragma once

#include <functional>
#include <stdint.h>
#include <string>

struct GLFWwindow;

namespace ST {
    struct DepthFrame;
    struct ColorFrame;
    struct InfraredFrame;
}

namespace SampleCode {
    namespace Gui {
        class RenderLoop {
        private:
            bool willRedrawNext = false;
        public:
            struct GLFWwindow *window = nullptr;

        public:
            RenderLoop();
            ~RenderLoop();
            RenderLoop(const RenderLoop&) = delete;
            RenderLoop(RenderLoop&&) = delete;
            RenderLoop& operator=(const RenderLoop&) = delete;
            RenderLoop& operator=(RenderLoop&&) = delete;
            using RenderCallback = std::function<bool()>;
            /** Run the main GUI loop until termination, using a single window
                with the given title and render function. Not thread-safe. */
            void main(const std::string& windowTitle, RenderCallback renderCallback);
            /** Call within the render callback to force the subsequent frame to
                render without waiting for input. For example, if the user
                presses a button that has side effects on other GUI elements,
                the other elements would not redraw until the user moves the
                mouse, unless the button calls this function when pressed. Not
                thread-safe. */
            void redrawNext();
            /** Wake the GUI loop from another thread. Thread-safe. */
            void wake();
            /** Obtain a reference to the render loop for this thread. */
            static RenderLoop& current();
        };

        /** A layout that has a central grid of equally-sized windows and a
            right sidebar containing vertically stacked utility windows. */
        struct GridConfig {
            unsigned numCellsX = 1;
            unsigned numCellsY = 1;
            unsigned numTools = 1;
            unsigned toolAreaWidth = 300;
        };
        /** Position and size the current top-level window (ImGui::Begin) as the grid window
            designated by the given 0-based indices. */
        void layoutWindowAsGridCell(const GridConfig& cfg, unsigned cellX, unsigned cellY);
        /** Position and size the current top-level window (ImGui::Begin) as the
            utility window designated by the given 0-based index. */
        void layoutWindowAsGridTool(const GridConfig& cfg, unsigned toolN);

        void drawTextureInContentArea(uint32_t textureId);
        void cameraFrameToTexture(uint32_t textureId, const ST::DepthFrame& frame);
        void cameraFrameToTexture(uint32_t textureId, const ST::ColorFrame& frame);
        void cameraFrameToTexture(uint32_t textureId, const ST::InfraredFrame& frame);
        void cameraFrameToTexture(uint32_t textureIdLeft, uint32_t textureIdRight, const ST::InfraredFrame& frame);
    }
}
