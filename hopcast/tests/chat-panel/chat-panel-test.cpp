// HopCast: checks the chat panel library loads, shows a page, refuses non-web links, remembers the link and keeps cookies only
// when asked. Run by CI with a local test page. GPL-2.0-or-later.
#include <QApplication>
#include <QLibrary>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWindow>
#include <QWebEngineProfile>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

static int bad = 0;
#define CHECK(c, msg) do { if (!(c)) { ++bad; printf("FAIL: %s\n", msg); } else printf("ok: %s\n", msg); fflush(stdout); } while (0)

// Usage: chat-panel-test <path to libhopcast-chat.so> <chat page link>
int main(int argc, char **argv)
{
	if (argc < 3) { printf("usage: chat-panel-test <library> <link>\n"); return 2; }
	QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
	QApplication app(argc, argv);
	QLibrary lib(argv[1]);
	CHECK(lib.load(), "library loads");
	auto abi = (int (*)())lib.resolve("hopcast_chat_abi");
	auto create = (QWidget * (*)(QWidget *)) lib.resolve("hopcast_chat_create");
	CHECK(abi && create && abi() == 1, "entry points found");
	// The choices come from Chat.ini (Settings > General > Chat writes it). Start with no link at all.
	QSettings s(QSettings::IniFormat, QSettings::UserScope, "HopCast", "Chat");
	s.clear(); s.sync();
	QWidget *panel = create(nullptr);
	panel->resize(420, 700);
	panel->show();
	auto *view = panel->findChild<QWebEngineView *>("chatView");
	auto *note = panel->findChild<QLabel *>("chatNote");
	CHECK(view && note, "panel has its page and note");
	CHECK(!panel->findChild<QLineEdit *>(), "no link box inside the panel (it is in Settings)");
	QTest::qWait(300);
	CHECK(note->isVisible() && note->text().contains("Settings"), "no link yet: the panel points to Settings");

	// the link set in Settings is noticed while the panel is open
	s.setValue("url", QString(argv[2])); s.sync();
	int n = 0; while (!view->title().startsWith("chat ok") && n++ < 150) QTest::qWait(100);
	printf("  title: %s\n", qPrintable(view->title()));
	CHECK(view->title().startsWith("chat ok"), "chat page loaded from the Settings link");
	CHECK(!note->isVisible() || !note->text().contains("Settings"), "hint gone once there is a link");

	// refuses things that aren't web links
	s.setValue("url", "javascript:alert(1)"); s.sync();
	QTest::qWait(1200);
	CHECK(view->url().toString() == QString(argv[2]), "javascript: link refused, page unchanged");
	CHECK(note->isVisible(), "refused link is explained");
	s.setValue("url", "file:///etc/passwd"); s.sync();
	QTest::qWait(1200);
	CHECK(view->url().scheme() != "file", "file: link refused");
	s.setValue("url", QString(argv[2])); s.sync();
	QTest::qWait(1200);

	// off the record by default; with "stay signed in" it keeps cookies in HopCast's own folder
	CHECK(view->page()->profile()->isOffTheRecord(), "no storage by default");
	s.setValue("signedIn", true); s.sync();
	n = 0; while (view->page()->profile()->isOffTheRecord() && n++ < 100) QTest::qWait(100);
	n = 0; while (!view->title().contains("cookie") && n++ < 100) QTest::qWait(100);
	CHECK(!view->page()->profile()->isOffTheRecord(), "stay signed in uses a stored profile");
	CHECK(view->title().contains("cookie"), "the signed-in page reloaded");

	// ---- the chat over a game (overlay): one page, borrowed from the panel; light when nothing is showing
	auto ovAbi = (int (*)())lib.resolve("hopcast_chat_overlay_abi");
	auto ovSet = (int (*)(int))lib.resolve("hopcast_chat_overlay_set");
	auto ovMove = (int (*)(int))lib.resolve("hopcast_chat_overlay_move");
	auto ovState = (int (*)())lib.resolve("hopcast_chat_overlay_state");
	CHECK(ovAbi && ovSet && ovMove && ovState && ovAbi() == 1, "overlay entry points found");
	CHECK(ovState() == 0, "overlay is off at start");
	auto runJs = [&](const QString &code) {
		QVariant out; bool done = false;
		view->page()->runJavaScript(code, [&](const QVariant &v) { out = v; done = true; });
		int k = 0; while (!done && k++ < 50) QTest::qWait(100);
		return out;
	};
	CHECK(view->page()->lifecycleState() == QWebEnginePage::LifecycleState::Active, "page is active while the panel shows");
	CHECK(ovSet(1) == 1 && ovState() == 1, "overlay turns on");
	QTest::qWait(500);
	QWidget *overlay = nullptr;
	for (QWidget *w : QApplication::topLevelWidgets()) if (w->objectName() == "hopcastChatOverlay") overlay = w;
	CHECK(overlay && overlay->isVisible(), "overlay window is shown");
	CHECK(overlay && (overlay->windowFlags() & Qt::WindowStaysOnTopHint) && (overlay->windowFlags() & Qt::FramelessWindowHint), "overlay is frameless and always on top");
	CHECK(overlay && overlay->windowTitle() == "HopCast Chat Overlay", "overlay has its fixed title (the KDE rule matches it)");
	CHECK(overlay && view->window() == overlay, "the one chat page now lives in the overlay (no second browser)");
	CHECK(overlay && overlay->windowHandle() && (overlay->windowHandle()->flags() & Qt::WindowTransparentForInput), "locked overlay is click-through");
	CHECK(view->title().startsWith("chat ok"), "chat is still the same loaded page");
	s.setValue("overlayOpacity", 50); s.sync();
	n = 0; while (qAbs(runJs("document.documentElement.style.opacity").toString().toDouble() - 0.5) > 0.001 && n++ < 30) QTest::qWait(200);
	CHECK(qAbs(runJs("document.documentElement.style.opacity").toString().toDouble() - 0.5) < 0.001, "see-through setting is applied to the overlay");
	CHECK(ovMove(1) == 1 && ovState() == 3, "move / resize mode turns on");
	CHECK(overlay->windowHandle() && !(overlay->windowHandle()->flags() & Qt::WindowTransparentForInput), "unlocked overlay takes the mouse so it can be moved");
	QWidget *drag = overlay->findChild<QWidget *>("chatOverlayDrag");
	CHECK(drag && drag->isVisible() && drag->size() == overlay->size(), "move mode: the drag layer covers the whole overlay");
	QPushButton *done = overlay->findChild<QPushButton *>("chatOverlayDone");
	CHECK(done && done->isVisible(), "move mode has a Done button on the overlay itself");
	if (done) done->click();
	QTest::qWait(200);
	CHECK(ovState() == 1 && (overlay->windowHandle()->flags() & Qt::WindowTransparentForInput), "Done locks it (click-through) without needing the dock's buttons");
	CHECK(ovMove(1) == 1, "move mode can be turned on again");
	CHECK(ovMove(0) == 0 && (overlay->windowHandle()->flags() & Qt::WindowTransparentForInput), "locking makes it click-through again");
	CHECK(drag && !drag->isVisible(), "locked: the drag layer is gone");
	panel->hide(); QTest::qWait(300);
	CHECK(view->page()->lifecycleState() == QWebEnginePage::LifecycleState::Active, "dock closed but overlay on: chat stays live");
	CHECK(ovSet(0) == 0 && ovState() == 0, "overlay turns off");
	QTest::qWait(400);
	CHECK(view->window() != overlay && !overlay->isVisible(), "page is back in the panel, overlay hidden");
	CHECK(view->page()->lifecycleState() == QWebEnginePage::LifecycleState::Frozen, "nothing showing: the page is frozen (no CPU)");
	panel->show(); QTest::qWait(400);
	CHECK(view->page()->lifecycleState() == QWebEnginePage::LifecycleState::Active, "panel shown again: page is live again");
	CHECK(qAbs(runJs("document.documentElement.style.opacity").toString().toDouble() - 1.0) < 0.001, "full strength again inside the panel");
	// geometry is remembered
	CHECK(ovSet(1) == 1, "overlay on again"); QTest::qWait(300);
	overlay->setGeometry(QRect(120, 140, 333, 444)); QTest::qWait(1200);
	CHECK(s.value("overlayRect").toString() == "120,140,333,444", "overlay position and size are remembered");
	ovSet(0);
	// no link: the overlay says so instead of being an invisible empty window
	s.remove("url"); s.sync(); QTest::qWait(1500);
	CHECK(ovSet(1) == 1, "overlay on without a link"); QTest::qWait(500);
	QLabel *hint = overlay->findChild<QLabel *>("chatOverlayHint");
	CHECK(hint && hint->isVisible() && hint->text().contains("Settings"), "overlay shows a hint when there is no chat link");
	ovSet(0);
	QTest::qWait(1500);
	delete panel;
	QTest::qWait(500);
	QDir d(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/HopCast/chat-profile");
	CHECK(d.exists(), "profile folder made only when asked");
	return bad ? 1 : 0;
}
