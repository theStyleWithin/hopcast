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

// The same, but only lines recorded since `sinceMs` (milliseconds since the epoch; see nowMs()).
QStringList recentConnectionLogSince(qint64 sinceMs, int maxLines);
qint64 nowMs();

} // namespace hopcast
