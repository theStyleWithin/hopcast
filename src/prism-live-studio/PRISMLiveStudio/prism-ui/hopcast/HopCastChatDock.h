// HopCast: the chat panel dock (paste a stream chat link; it shows beside the preview). GPL-2.0-or-later.
#pragma once

class QDockWidget;
class QMainWindow;

namespace hopcast {

// Makes the "Chat" dock, hidden at first, in the window's right dock area (drag it to the left if you prefer). The browser
// engine behind it lives in a separate library that is only loaded when the dock is first opened, so HopCast starts and
// runs the same where that library is missing; the dock then says so instead of showing a chat.
QDockWidget *createChatDock(QMainWindow *window);

// The chat over a game: a see-through, always-on-top, click-through window with the same chat page (the page is moved out
// of the dock while it shows, so there is never a second browser). Never on at start-up; you turn it on yourself.
// `move` lets you drag and resize it; leave it off (locked) and mouse and keys go to the game underneath.
bool chatOverlayAvailable();
bool chatOverlayOn();
bool chatOverlayMove();
bool setChatOverlay(bool on);        // returns the state afterwards (false if the chat panel can't start here)
bool setChatOverlayMove(bool move);  // returns the state afterwards

} // namespace hopcast
