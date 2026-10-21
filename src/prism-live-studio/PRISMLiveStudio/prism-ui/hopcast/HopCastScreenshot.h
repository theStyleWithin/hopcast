// HopCast: desktop screenshots for the system tray menu. GPL-2.0-or-later.
#pragma once

#include <QString>
#include <functional>

namespace hopcast {

// Saves a screenshot of the whole desktop to <Pictures>/HopCast. On X11 the screen is read directly; on
// Wayland (and when X11 fails) the xdg-desktop-portal Screenshot service is used, which may ask the user once.
// `done` is called once, on the calling thread's event loop: `path` is set on success, `error` otherwise.
void takeDesktopScreenshot(std::function<void(const QString &path, const QString &error)> done);

} // namespace hopcast
