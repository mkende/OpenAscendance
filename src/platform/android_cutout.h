// Android: the display cutout (camera hole, notch) of the game's window,
// from GameActivity (android/java). A remake addition.
#pragma once

#include <array>

namespace oa {

// Returns the window's safe insets from the display cutout (left, top,
// right, bottom), in pixels; zeros when there is no cutout or it is not
// known (yet).
std::array<int, 4> android_cutout_insets();

}  // namespace oa
