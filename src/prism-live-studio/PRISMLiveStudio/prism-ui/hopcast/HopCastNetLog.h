// HopCast: remembers the last few network/RTMP log lines so a failed stream can show what the server said
// instead of only an error number. GPL-2.0-or-later.
#pragma once

#include <QString>
#include <QStringList>

namespace hopcast {

// Called for every log line (any thread); keeps only the lines about connections.
void recordLogLine(const char *line);

// The most recent connection lines, oldest first, with stream keys hidden.
QStringList recentConnectionLog(int maxLines);

} // namespace hopcast
