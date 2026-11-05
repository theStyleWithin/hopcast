// HopCast: where the chat panel's few choices are kept (Settings > General > Chat writes them, the Chat panel reads them).
// GPL-2.0-or-later.
#pragma once

#include <QSettings>
#include <QString>
#include <QUrl>

namespace hopcast {

// One small file, ~/.config/HopCast/Chat.ini (the Windows equivalent there). The chat panel library opens the same file.
inline QSettings openChatSettings()
{
	return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("HopCast"), QStringLiteral("Chat"));
}

// http(s) only. A bare "kick.com/..." gets https:// in front. Anything else (file:, javascript:, ...) is refused: the
// result is invalid (empty).
inline QUrl cleanChatLink(const QString &text)
{
	QString t = text.trimmed();
	if (t.isEmpty() || t.contains(QLatin1Char(' ')))
		return QUrl();
	if (!t.contains(QLatin1String("://")))
		t = QStringLiteral("https://") + t;
	const QUrl u(t, QUrl::StrictMode);
	if (!u.isValid() || u.host().isEmpty() || (u.scheme() != QLatin1String("http") && u.scheme() != QLatin1String("https")))
		return QUrl();
	return u;
}

} // namespace hopcast
