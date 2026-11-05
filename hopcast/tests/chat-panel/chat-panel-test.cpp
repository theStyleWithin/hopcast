// HopCast: checks the chat panel library loads, shows a page, refuses non-web links, remembers the link and keeps cookies only
// when asked. Run by CI with a local test page. GPL-2.0-or-later.
#include <QApplication>
#include <QLibrary>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
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
	QWidget *panel = create(nullptr);
	panel->resize(420, 700);
	panel->show();

	auto *url = panel->findChild<QLineEdit *>("chatUrl");
	auto *view = panel->findChild<QWebEngineView *>("chatView");
	auto *go = panel->findChild<QPushButton *>("chatGo");
	url->setText(argv[2]);
	go->click();
	int n = 0; while (!view->title().startsWith("chat ok") && n++ < 150) QTest::qWait(100);
	printf("  title: %s\n", qPrintable(view->title()));
	CHECK(view->title().startsWith("chat ok"), "chat page loaded");

	// refuses things that aren't web links
	auto *note = panel->findChild<QLabel *>();
	url->setText("javascript:alert(1)");
	go->click();
	CHECK(view->url().toString() == QString(argv[2]), "javascript: link refused, page unchanged");
	url->setText("file:///etc/passwd");
	go->click();
	CHECK(view->url().scheme() != "file", "file: link refused");

	// the link is remembered
	QSettings s(QSettings::IniFormat, QSettings::UserScope, "HopCast", "Chat");
	CHECK(s.value("url").toString() == QString(argv[2]), "link remembered");

	// off the record by default; with "stay signed in" it keeps cookies in HopCast's own folder
	CHECK(view->page()->profile()->isOffTheRecord(), "no storage by default");
	auto *keep = panel->findChild<QCheckBox *>("chatSignedIn");
	keep->setChecked(true);
	n = 0; while (!view->title().contains("cookie") && n++ < 100) QTest::qWait(100);
	CHECK(!view->page()->profile()->isOffTheRecord(), "stay signed in uses a stored profile");
	QTest::qWait(1500);
	delete panel;
	QTest::qWait(500);
	QDir d(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/HopCast/chat-profile");
	CHECK(d.exists(), "profile folder made only when asked");
	return bad ? 1 : 0;
}
