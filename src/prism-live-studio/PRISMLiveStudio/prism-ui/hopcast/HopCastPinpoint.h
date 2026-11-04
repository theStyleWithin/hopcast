// HopCast: HopShots "pinpoint" screenshot. GPL-2.0-or-later.
#pragma once

#include <QString>
#include <functional>

namespace hopcast {

// Takes a picture of the desktop, freezes it, and lets you point at one picture (or any block) on the screen: HopCast
// outlines the whole picture under the pointer, pixel exact, so no cropping is needed. Click to save it as a PNG in
// <Pictures>/HopCast (it is also put on the clipboard). Scroll or +/- picks a bigger or smaller box, dragging draws your own,
// Esc cancels. Only screen pixels are used, so nothing is downloaded and the saved file is a fresh PNG.
// `done` is called once: `path` is set on success, `error` on a failure, and both are empty if you cancelled.
void takePinpointScreenshot(std::function<void(const QString &path, const QString &error)> done);

} // namespace hopcast
