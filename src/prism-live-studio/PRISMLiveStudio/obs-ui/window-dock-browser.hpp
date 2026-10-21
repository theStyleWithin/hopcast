#pragma once

#include "window-dock.hpp"
#include <QScopedPointer>
#include "libui.h"

#include <browser-panel.hpp>
extern QCef *cef;
extern QCefCookieManager *panel_cookies;

class BrowserDock : public OBSDock {
private:
	QString title;

public:
	inline BrowserDock() : OBSDock()
	{
		pls_add_css(this, {"BrowserDock"});
#if !defined(Q_OS_LINUX) // HopCast: native child widgets leave stale paint on Linux
		setAttribute(Qt::WA_NativeWindow);
#endif
	}
	inline BrowserDock(const QString &title_) : OBSDock(title_)
	{
		pls_add_css(this, {"BrowserDock"});
		title = title_;
#if !defined(Q_OS_LINUX) // HopCast: native child widgets leave stale paint on Linux
		setAttribute(Qt::WA_NativeWindow);
#endif
	}

	QScopedPointer<QCefWidget> cefWidget;

	inline void SetWidget(QCefWidget *widget_)
	{
		setWidget(widget_);
		cefWidget.reset(widget_);
	}

	inline void setTitle(const QString &title_) { title = title_; }

	void closeEvent(QCloseEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void closeBrowser();
};
