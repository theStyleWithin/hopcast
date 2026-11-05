// HopCast: checks the chat panel library loads, shows a page, refuses non-web links, remembers the link and keeps cookies only
// when asked. Run by CI with a local test page. GPL-2.0-or-later.
#include <QApplication>
#include <QLibrary>
#include <QLineEdit>
#include <QLabel>
#include <QTest>
#include <QWebEngineView>
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
	QTest::qWait(1500);
	delete panel;
	QTest::qWait(500);
	QDir d(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/HopCast/chat-profile");
	CHECK(d.exists(), "profile folder made only when asked");
	return bad ? 1 : 0;
}
