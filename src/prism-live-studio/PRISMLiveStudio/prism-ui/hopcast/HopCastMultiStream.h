// HopCast: stream to several servers at once, without a relay. GPL-2.0-or-later.
//
// The first channel that is switched on streams through the normal stream output. Every other
// channel gets one more output that shares that output's video and audio encoders, so the picture
// is encoded once and sent to every server. A failing destination never stops the others.
#pragma once

#include <QString>
#include <functional>
#include <vector>

struct obs_output;
typedef struct obs_output obs_output_t;

namespace hopcast {

struct Destination {
	QString name;     // channel name, used in messages
	QString server;   // rtmp://, rtmps://, srt:// or rist:// address
	QString key;      // stream key (may be empty for SRT/RIST)
	QString user;     // optional RTMP user name
	QString password; // optional RTMP password
};

// Remembers the extra destinations for the next stream (horizontal output, or vertical with dual output).
void setExtraDestinations(bool vertical, std::vector<Destination> list);

// Call once `mainOutput` is streaming. Starts one output per remembered destination.
// `notify` is called on the GUI thread when a destination fails to start or stops with an error.
void startExtraDestinations(bool vertical, obs_output_t *mainOutput, int maxRetries, int retryDelaySec, std::function<void(const QString &message)> notify);

// Gracefully stops every extra destination. Safe to call at any time, and more than once.
void stopExtraDestinations();

} // namespace hopcast
