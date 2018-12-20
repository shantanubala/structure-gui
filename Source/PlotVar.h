#include <ImGui/imgui.h>

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
void PlotVar(const char *label, float value, float scale_min = FLT_MAX, float scale_max = FLT_MAX, size_t buffer_size = 120);

/**
 * @brief Flush all of the existing plotted items from the buffer.
 *
 */
void PlotVarFlushOldEntries();