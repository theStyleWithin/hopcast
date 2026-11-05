// HopCast: the chat panel dock (paste a stream chat link; it shows beside the preview). GPL-2.0-or-later.
#pragma once

class QDockWidget;
class QMainWindow;

namespace hopcast {

// Makes the "Chat" dock, hidden at first, in the window's right dock area (drag it to the left if you prefer). The browser
// engine behind it lives in a separate library that is only loaded when the dock is first opened, so HopCast starts and
// runs the same where that library is missing; the dock then says so instead of showing a chat.
QDockWidget *createChatDock(QMainWindow *window);

} // namespace hopcast
