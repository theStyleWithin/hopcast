// HopCast: download a video from a link (tray menu and Tools menu). GPL-2.0-or-later.
#pragma once

class QWidget;

namespace hopcast {

// Opens the "Download a video" window. It runs the official yt-dlp program (the same engine Video Downloader uses) in a
// separate process, so nothing from a website runs inside HopCast. yt-dlp and ffmpeg are looked up on the system; if
// yt-dlp is missing, the window offers to fetch the official release once, and checks its checksum first.
void showVideoDownloader(QWidget *parent);

} // namespace hopcast
