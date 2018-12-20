#include "PlotVar.h"
#include <map>
#include <algorithm>

/**
 * @brief Data plotted in a sparkline
 *
 */
struct PlotVarData
{
    ImGuiID ID; /**< A unique identifier for the data to plot*/
    ImVector<float> Data; /**< The data being plotted */
    int DataInsertIdx; /**< The index within the array of data */
    int LastFrame; /**< The last frame at which the data was updated */

    PlotVarData() : ID(0), DataInsertIdx(0), LastFrame(-1) {}
};

typedef std::map<ImGuiID, PlotVarData> PlotVarsMap;
static PlotVarsMap g_PlotVarsMap;

// Plot value over time
//

/**
 * @brief Plot a value over time in a sparkline.
 *
 * This function plots a value over time on a sparkline by adding to a buffer. To update,
 * call with 'value == FLT_MAX' to draw without adding new values to the buffer.
 *
 * @param label - the label for the plot
 * @param value - the next value
 * @param scale_min - the minimum bounds of the plot
 * @param scale_max - the maximum bounds of the plot
 * @param buffer_size - the size of the buffer
 */
void PlotVar(const char *label, float value, float scale_min, float scale_max, size_t buffer_size)
{
    if (buffer_size == 0)
        buffer_size = 400;

    ImGui::PushID(label);
    ImGuiID id = ImGui::GetID("");

    // Lookup O(log N)
    PlotVarData &pvd = g_PlotVarsMap[id];

    // Setup
    if (pvd.Data.capacity() != buffer_size)
    {
        pvd.Data.resize(buffer_size);
        memset(&pvd.Data[0], 0, sizeof(float) * buffer_size);
        pvd.DataInsertIdx = 0;
        pvd.LastFrame = -1;
    }

    // Insert (avoid unnecessary modulo operator)
    if (pvd.DataInsertIdx == buffer_size)
        pvd.DataInsertIdx = 0;
    int display_idx = pvd.DataInsertIdx;
    if (value != FLT_MAX)
        pvd.Data[pvd.DataInsertIdx++] = value;

    // Draw
    int current_frame = ImGui::GetFrameCount();
    if (pvd.LastFrame != current_frame)
    {
        ImGui::PlotLines("##plot", &pvd.Data[0], buffer_size, pvd.DataInsertIdx, NULL, scale_min, scale_max, ImVec2(0, 40));
        ImGui::SameLine();
        ImGui::Text("%s\n%-3.4f", label, pvd.Data[display_idx]); // Display last value in buffer
        pvd.LastFrame = current_frame;
    }

    ImGui::PopID();
}

void PlotVarFlushOldEntries()
{
    int current_frame = ImGui::GetFrameCount();
    for (PlotVarsMap::iterator it = g_PlotVarsMap.begin(); it != g_PlotVarsMap.end();)
    {
        PlotVarData &pvd = it->second;
        if (pvd.LastFrame < current_frame - std::max(400, (int)pvd.Data.size()))
            it = g_PlotVarsMap.erase(it);
        else
            ++it;
    }
}