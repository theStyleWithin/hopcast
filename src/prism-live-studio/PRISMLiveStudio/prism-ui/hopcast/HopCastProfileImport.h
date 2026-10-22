// HopCast: import an OBS profile (a .zip or a folder with service.json, basic.ini, streamEncoder.json),
// such as the ones streaming sites hand out. GPL-2.0-or-later.
#pragma once

#include <QString>

class QWidget;

namespace hopcast {

// Reads the profile at `path`, adds a Custom RTMP channel from its server and stream key (the key goes into
// HopCast's encrypted store like any other), and saves its video/encoder settings as a profile of the same
// name (never service.json: that file holds the key in plain text). Shows what it did, or why it couldn't.
void importObsProfile(QWidget *parent, const QString &path);

} // namespace hopcast
