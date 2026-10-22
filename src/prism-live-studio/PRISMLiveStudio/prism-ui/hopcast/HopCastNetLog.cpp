// HopCast: recent connection log lines. GPL-2.0-or-later.
#include "HopCastNetLog.h"

#include <QRegularExpression>
#include <QMutex>
#include <QMutexLocker>
#include <cstring>
#include <deque>

namespace {

QMutex &mutex()
{
	static QMutex *m = new QMutex; // never destroyed: logging can run during exit
	return *m;
}

std::deque<QString> &lines()
{
	static auto *d = new std::deque<QString>;
	return *d;
}

bool interesting(const char *s)
{
	static const char *const words[] = {"RTMP", "rtmp", "SRT", "srt", "TLS", "tls", "SSL", "ssl", "Connecting",
					    "Connection to", "handshake", "Handshake", "certificate", "Disconnected"};
	for (const char *w : words)
		if (strstr(s, w))
			return true;
	return false;
}

// Never let a key reach the screen: hide anything shaped like one, whatever the site.
QString hideKeys(QString s)
{
	static const QRegularExpression keyShaped(QStringLiteral("(sk_|live_|rtmp[s]?://[^\\s/]+/[^\\s/]*/)[A-Za-z0-9_\\-?=&.]{8,}"));
	static const QRegularExpression longToken(QStringLiteral("[A-Za-z0-9_\\-]{32,}"));
	s.replace(keyShaped, QStringLiteral("\\1…"));
	s.replace(longToken, QStringLiteral("…"));
	return s;
}

} // namespace

namespace hopcast {

void recordLogLine(const char *line)
{
	if (!line || !interesting(line))
		return;
	QMutexLocker lock(&mutex());
	auto &d = lines();
	d.push_back(QString::fromUtf8(line).trimmed().left(300));
	while (d.size() > 40)
		d.pop_front();
}

QStringList recentConnectionLog(int maxLines)
{
	QMutexLocker lock(&mutex());
	const auto &d = lines();
	QStringList out;
	const int start = qMax(0, static_cast<int>(d.size()) - maxLines);
	for (int i = start; i < static_cast<int>(d.size()); ++i)
		out << hideKeys(d[i]);
	return out;
}

} // namespace hopcast
