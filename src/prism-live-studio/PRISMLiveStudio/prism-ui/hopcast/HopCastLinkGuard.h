// HopCast: keeps the app from opening PRISM/NAVER web pages in the browser.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QUrl>

namespace hopcast {

/* Routes every QDesktopServices::openUrl() for http/https through one check.
 * Links to PRISM/NAVER sites (the same domain list the network block uses) are
 * dropped and logged; everything else opens in the user's browser as usual.
 * Catches links left in code paths HopCast has not removed one by one. */
class LinkGuard : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
	static void install();

public slots:
	void openUrl(const QUrl &url);
};

} // namespace hopcast
