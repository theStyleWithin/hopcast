// SPDX-License-Identifier: GPL-2.0-or-later
#include "HopCastLinkGuard.h"

#include <libhttp-client.h>
#include <liblog.h>

#include <QCoreApplication>
#include <QDesktopServices>

namespace hopcast {

static constexpr auto MODULE = "HopCastLinkGuard";
static const char *const kSchemes[] = {"http", "https"};

void LinkGuard::install()
{
	static LinkGuard *guard = nullptr;
	if (guard)
		return;
	guard = new LinkGuard(QCoreApplication::instance());
	for (const char *scheme : kSchemes)
		QDesktopServices::setUrlHandler(QString::fromLatin1(scheme), guard, "openUrl");
}

void LinkGuard::openUrl(const QUrl &url)
{
	if (pls::http::pls_hopcast_is_blocked_url(url)) {
		PLS_INFO(MODULE, "HopCast: not opening PRISM/NAVER page (%s)", qUtf8Printable(url.host()));
		return;
	}
	/* Hand the URL to the desktop: drop our handler for this call so Qt uses the
	 * system browser, then put it back. Runs on the GUI thread (Qt queues it). */
	const QString scheme = url.scheme().toLower();
	QDesktopServices::unsetUrlHandler(scheme);
	QDesktopServices::openUrl(url);
	QDesktopServices::setUrlHandler(scheme, this, "openUrl");
}

} // namespace hopcast
