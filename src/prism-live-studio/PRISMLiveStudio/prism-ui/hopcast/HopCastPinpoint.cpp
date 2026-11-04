// HopCast: HopShots "pinpoint" screenshot. GPL-2.0-or-later.
#include "HopCastPinpoint.h"
#include "HopCastScreenshot.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QStandardPaths>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>
#include <QFontMetrics>
#include <QPen>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {

using Done = std::function<void(const QString &, const QString &)>;

QString newPinpointPath()
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
	if (dir.isEmpty())
		dir = QDir::homePath();
	dir += QStringLiteral("/HopCast");
	QDir().mkpath(dir);
	return dir + QStringLiteral("/Pinpoint ") +
	       QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh-mm-ss")) + QStringLiteral(".png");
}

struct Candidate {
	QRect box;    // in picture pixels
	float frame;  // how much of each side of the box has an edge on it: a picture against the page does
};

/*
 * Finds the block of the screen under a point. A pixel is "busy" where the colour changes (a picture's pixels, its edge
 * against the page, text) and "quiet" on flat page background. Busy pixels that sit close together form one block; a few
 * different closeness settings give a small box (one picture) up to a big one (the picture with its caption, a whole card).
 */
class PinSnap {
public:
	explicit PinSnap(const QImage &colour)
	{
		gray_ = colour.convertToFormat(QImage::Format_Grayscale8);
		w_ = gray_.width();
		h_ = gray_.height();
		f_ = w_ > 2600 ? 3 : 2;
		sw_ = w_ / f_;
		sh_ = h_ / f_;
		if (sw_ < 8 || sh_ < 8)
			return;

		std::vector<uint8_t> small(size_t(sw_) * sh_);
		for (int y = 0; y < sh_; ++y) {
			for (int x = 0; x < sw_; ++x) {
				int sum = 0;
				for (int dy = 0; dy < f_; ++dy) {
					const uchar *row = gray_.constScanLine(y * f_ + dy) + x * f_;
					for (int dx = 0; dx < f_; ++dx)
						sum += row[dx];
				}
				small[size_t(y) * sw_ + x] = uint8_t(sum / (f_ * f_));
			}
		}

		std::vector<uint8_t> busy(small.size(), 0);
		for (int y = 0; y < sh_; ++y) {
			for (int x = 0; x < sw_; ++x) {
				const size_t i = size_t(y) * sw_ + x;
				const int dx = x + 1 < sw_ ? std::abs(int(small[i + 1]) - int(small[i])) : 0;
				const int dy = y + 1 < sh_ ? std::abs(int(small[i + sw_]) - int(small[i])) : 0;
				busy[i] = (dx + dy) > kBusy ? 1 : 0;
			}
		}

		for (int r : {0, 1, 2, 4, 8})
			findBlocks(busy, dilate(busy, r));
	}

	/** Every block under the point, smallest first. */
	QList<Candidate> at(const QPoint &p) const
	{
		QList<Candidate> out;
		for (const Candidate &c : blocks_) {
			if (!c.box.contains(p))
				continue;
			bool same = false;
			for (Candidate &o : out) {
				if (near(o.box, c.box)) {
					o.frame = std::max(o.frame, c.frame);
					same = true;
					break;
				}
			}
			if (!same)
				out.push_back(c);
		}
		std::sort(out.begin(), out.end(), [](const Candidate &a, const Candidate &b) { return area(a.box) < area(b.box); });
		return out;
	}

	/** The box to offer first: the smallest one that looks like a picture, else the smallest that isn't tiny. */
	static int defaultIndex(const QList<Candidate> &list, const QSize &screen)
	{
		const qint64 max = qint64(screen.width()) * screen.height() * 6 / 10;
		int firstBig = -1;
		for (int i = 0; i < list.size(); ++i) {
			const qint64 a = area(list[i].box);
			if (a < 48 * 48 || a > max)
				continue;
			if (list[i].frame >= kFramed)
				return i;
			if (firstBig < 0)
				firstBig = i;
		}
		return firstBig >= 0 ? firstBig : 0;
	}

	/** Moves each side of the box onto the exact pixel row or column where the picture starts or ends. */
	QRect refine(const QRect &box) const
	{
		if (gray_.isNull())
			return box;
		const int reach = f_ * 2; // the block search is off by at most a block or so
		const int m = 4; // keep away from the corners, where rounded corners and shadows are
		const int x0 = box.left(), y0 = box.top(), x1 = box.right() + 1, y1 = box.bottom() + 1;

		auto colScore = [&](int x) {
			if (x < 1 || x >= w_)
				return 0.0;
			double sum = 0;
			int n = 0;
			for (int y = std::max(0, y0 + m); y < std::min(h_, y1 - m); y += 2) {
				const uchar *row = gray_.constScanLine(y);
				sum += std::abs(int(row[x]) - int(row[x - 1]));
				++n;
			}
			return n ? sum / n : 0.0;
		};
		auto rowScore = [&](int y) {
			if (y < 1 || y >= h_)
				return 0.0;
			const uchar *a = gray_.constScanLine(y);
			const uchar *b = gray_.constScanLine(y - 1);
			double sum = 0;
			int n = 0;
			for (int x = std::max(0, x0 + m); x < std::min(w_, x1 - m); x += 2) {
				sum += std::abs(int(a[x]) - int(b[x]));
				++n;
			}
			return n ? sum / n : 0.0;
		};
		// the strongest edge near the guess; on a tie the one closest to the guess
		auto best = [&](int guess, auto score, int limit) {
			int bestPos = guess;
			double bestVal = -1;
			for (int c = std::max(1, guess - reach); c <= std::min(limit, guess + reach); ++c) {
				const double v = score(c) * (1.0 - 0.06 * std::abs(c - guess));
				if (v > bestVal) {
					bestVal = v;
					bestPos = c;
				}
			}
			return bestPos;
		};

		const int nx0 = best(x0, colScore, w_ - 1);
		const int nx1 = best(x1, colScore, w_);
		const int ny0 = best(y0, rowScore, h_ - 1);
		const int ny1 = best(y1, rowScore, h_);
		if (nx1 - nx0 < 4 || ny1 - ny0 < 4)
			return box;
		return QRect(nx0, ny0, nx1 - nx0, ny1 - ny0);
	}

private:
	static constexpr int kBusy = 9;       // colour change (0-255, two directions added) that counts as busy
	static constexpr float kFramed = 0.6f; // a picture has an edge along at least this much of every side

	static qint64 area(const QRect &r) { return qint64(r.width()) * r.height(); }

	static bool near(const QRect &a, const QRect &b)
	{
		return std::abs(a.left() - b.left()) <= 3 && std::abs(a.top() - b.top()) <= 3 &&
		       std::abs(a.right() - b.right()) <= 3 && std::abs(a.bottom() - b.bottom()) <= 3;
	}

	// Grows every busy pixel by r in each direction (a square), using running sums.
	std::vector<uint8_t> dilate(const std::vector<uint8_t> &m, int r) const
	{
		if (r == 0)
			return m;
		std::vector<uint8_t> horiz(m.size(), 0);
		std::vector<int> sum(size_t(std::max(sw_, sh_)) + 1);
		for (int y = 0; y < sh_; ++y) {
			sum[0] = 0;
			for (int x = 0; x < sw_; ++x)
				sum[x + 1] = sum[x] + m[size_t(y) * sw_ + x];
			for (int x = 0; x < sw_; ++x) {
				const int lo = std::max(0, x - r), hi = std::min(sw_, x + r + 1);
				horiz[size_t(y) * sw_ + x] = sum[hi] - sum[lo] > 0 ? 1 : 0;
			}
		}
		std::vector<uint8_t> out(m.size(), 0);
		for (int x = 0; x < sw_; ++x) {
			sum[0] = 0;
			for (int y = 0; y < sh_; ++y)
				sum[y + 1] = sum[y] + horiz[size_t(y) * sw_ + x];
			for (int y = 0; y < sh_; ++y) {
				const int lo = std::max(0, y - r), hi = std::min(sh_, y + r + 1);
				out[size_t(y) * sw_ + x] = sum[hi] - sum[lo] > 0 ? 1 : 0;
			}
		}
		return out;
	}

	// Labels the connected blocks of `mask` and keeps, for each, the box around its own busy pixels and its frame score.
	void findBlocks(const std::vector<uint8_t> &busy, const std::vector<uint8_t> &mask)
	{
		std::vector<uint8_t> seen(mask.size(), 0);
		std::vector<int> stack;
		std::vector<int> members;
		for (int sy = 0; sy < sh_; ++sy) {
			for (int sx = 0; sx < sw_; ++sx) {
				const size_t start = size_t(sy) * sw_ + sx;
				if (!mask[start] || seen[start])
					continue;
				members.clear();
				stack.clear();
				stack.push_back(int(start));
				seen[start] = 1;
				while (!stack.empty()) {
					const int i = stack.back();
					stack.pop_back();
					if (busy[i])
						members.push_back(i);
					const int x = i % sw_, y = i / sw_;
					const int nbr[4] = {x > 0 ? i - 1 : -1, x + 1 < sw_ ? i + 1 : -1, y > 0 ? i - sw_ : -1,
							    y + 1 < sh_ ? i + sw_ : -1};
					for (int n : nbr) {
						if (n >= 0 && mask[n] && !seen[n]) {
							seen[n] = 1;
							stack.push_back(n);
						}
					}
				}
				if (members.empty())
					continue;
				int x0 = sw_, y0 = sh_, x1 = 0, y1 = 0;
				for (int i : members) {
					const int x = i % sw_, y = i / sw_;
					x0 = std::min(x0, x);
					x1 = std::max(x1, x + 1);
					y0 = std::min(y0, y);
					y1 = std::max(y1, y + 1);
				}
				const int bw = x1 - x0, bh = y1 - y0;
				if (bw * f_ < 12 || bh * f_ < 12)
					continue;

				std::vector<uint8_t> top(bw, 0), bottom(bw, 0), left(bh, 0), right(bh, 0);
				for (int i : members) {
					const int x = i % sw_ - x0, y = i / sw_ - y0;
					if (y < 2)
						top[x] = 1;
					if (bh - 1 - y < 2)
						bottom[x] = 1;
					if (x < 2)
						left[y] = 1;
					if (bw - 1 - x < 2)
						right[y] = 1;
				}
				auto share = [](const std::vector<uint8_t> &v) {
					size_t n = 0;
					for (uint8_t b : v)
						n += b;
					return float(n) / float(v.size());
				};
				const float frame = std::min(std::min(share(top), share(bottom)), std::min(share(left), share(right)));
				blocks_.push_back({QRect(x0 * f_, y0 * f_, bw * f_, bh * f_), frame});
			}
		}
	}

	QImage gray_;
	int w_ = 0, h_ = 0, f_ = 2, sw_ = 0, sh_ = 0;
	QList<Candidate> blocks_;
};

// The part of the desktop picture that belongs to one screen (the picture may cover all screens side by side).
QRect cropFor(const QImage &img, QScreen *screen)
{
	const qreal dpr = screen->devicePixelRatio();
	const QSize want(qRound(screen->geometry().width() * dpr), qRound(screen->geometry().height() * dpr));
	auto close = [](const QSize &a, const QSize &b) { return qAbs(a.width() - b.width()) <= 2 && qAbs(a.height() - b.height()) <= 2; };
	if (close(img.size(), want))
		return img.rect();

	QRect all;
	qreal ratio = 1.0;
	for (QScreen *s : QGuiApplication::screens()) {
		all = all.united(s->geometry());
		ratio = qMax(ratio, s->devicePixelRatio());
	}
	const QSize wantAll(qRound(all.width() * ratio), qRound(all.height() * ratio));
	if (close(img.size(), wantAll)) {
		const QPoint off(qRound((screen->geometry().left() - all.left()) * ratio), qRound((screen->geometry().top() - all.top()) * ratio));
		return QRect(off, want).intersected(img.rect());
	}
	return img.rect();
}

class PinpointOverlay : public QWidget {
public:
	PinpointOverlay(const QImage &image, QScreen *screen, Done done) : done_(std::move(done))
	{
		view_ = image.copy(cropFor(image, screen)).convertToFormat(QImage::Format_RGB32);
		snap_ = std::make_unique<PinSnap>(view_);

		setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
		setAttribute(Qt::WA_DeleteOnClose);
		setMouseTracking(true);
		setCursor(Qt::CrossCursor);
		setFocusPolicy(Qt::StrongFocus);
		setGeometry(screen->geometry());
		create();
		if (windowHandle())
			windowHandle()->setScreen(screen);
		cursor_ = toImage(mapFromGlobal(QCursor::pos()));
		refresh(true);
		showFullScreen();
		raise();
		activateWindow();
		setFocus();
	}

	// For tests and for the app: where the box is right now, in picture pixels.
	QRect currentBox() const { return current(); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.drawImage(rect(), view_);
		p.fillRect(rect(), QColor(0, 0, 0, 140));

		const QRect box = current();
		if (!box.isEmpty()) {
			const QRectF target = toWidget(box);
			p.drawImage(target, view_, QRectF(box));
			p.setPen(QPen(QColor(0xef, 0xfc, 0x35), 2));
			p.drawRect(target);

			const QString label = QStringLiteral("%1 x %2").arg(box.width()).arg(box.height());
			QFontMetrics fm(p.font());
			const QRect textBox(0, 0, fm.horizontalAdvance(label) + 12, fm.height() + 6);
			QPointF at(target.left(), target.top() - textBox.height() - 2);
			if (at.y() < 0)
				at.setY(target.top() + 2);
			p.fillRect(QRectF(at, QSizeF(textBox.size())), QColor(0, 0, 0, 200));
			p.setPen(Qt::white);
			p.drawText(QRectF(at, QSizeF(textBox.size())), Qt::AlignCenter, label);
		}

		const QString help = QStringLiteral("Click to save this picture   |   Scroll or + / - for a bigger or smaller box   |   Drag to draw your own   |   Esc to cancel");
		QFontMetrics fm(p.font());
		const QRect bar(0, 0, width(), fm.height() + 14);
		p.fillRect(bar, QColor(0, 0, 0, 210));
		p.setPen(Qt::white);
		p.drawText(bar, Qt::AlignCenter, help);
	}

	void mouseMoveEvent(QMouseEvent *e) override
	{
		const QPointF pos = e->position();
		if (pressed_) {
			if ((pos - pressPos_).manhattanLength() > 8)
				dragging_ = true;
			if (dragging_)
				manual_ = QRect(toImage(pressPos_), toImage(pos)).normalized();
		} else {
			cursor_ = toImage(pos);
			refresh(false);
		}
		update();
	}

	void mousePressEvent(QMouseEvent *e) override
	{
		if (e->button() != Qt::LeftButton) {
			finish(QString(), QString());
			return;
		}
		pressed_ = true;
		dragging_ = false;
		pressPos_ = e->position();
		manual_ = QRect();
	}

	void mouseReleaseEvent(QMouseEvent *e) override
	{
		if (e->button() != Qt::LeftButton || !pressed_)
			return;
		pressed_ = false;
		if (dragging_ && manual_.width() >= 6 && manual_.height() >= 6)
			save(manual_.intersected(view_.rect()));
		else if (!dragging_)
			save(current());
		dragging_ = false;
	}

	void wheelEvent(QWheelEvent *e) override
	{
		step(e->angleDelta().y() > 0 ? 1 : -1);
		e->accept();
	}

	void keyPressEvent(QKeyEvent *e) override
	{
		switch (e->key()) {
		case Qt::Key_Escape:
			finish(QString(), QString());
			break;
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Space:
			save(current());
			break;
		case Qt::Key_Plus:
		case Qt::Key_Equal:
		case Qt::Key_Up:
			step(1);
			break;
		case Qt::Key_Minus:
		case Qt::Key_Down:
			step(-1);
			break;
		default:
			QWidget::keyPressEvent(e);
		}
	}

	void closeEvent(QCloseEvent *e) override
	{
		finish(QString(), QString());
		e->accept();
	}

private:
	QPoint toImage(const QPointF &w) const
	{
		const double sx = double(view_.width()) / std::max(1, width());
		const double sy = double(view_.height()) / std::max(1, height());
		return QPoint(qBound(0, int(w.x() * sx), view_.width() - 1), qBound(0, int(w.y() * sy), view_.height() - 1));
	}

	QRectF toWidget(const QRect &r) const
	{
		const double sx = double(width()) / std::max(1, view_.width());
		const double sy = double(height()) / std::max(1, view_.height());
		return QRectF(r.left() * sx, r.top() * sy, r.width() * sx, r.height() * sy);
	}

	// The box to show: your own while dragging, else the offered block (a little nudged by scrolling), pixel exact.
	QRect current() const
	{
		if (dragging_ && !manual_.isEmpty())
			return manual_.intersected(view_.rect());
		if (cands_.isEmpty())
			return QRect();
		const int i = qBound(0, base_ + bias_, int(cands_.size()) - 1);
		return snap_->refine(cands_[i].box).intersected(view_.rect());
	}

	void refresh(bool force)
	{
		if (!force && (cursor_ - biasPoint_).manhattanLength() > 24)
			bias_ = 0;
		cands_ = snap_->at(cursor_);
		base_ = cands_.isEmpty() ? 0 : PinSnap::defaultIndex(cands_, view_.size());
	}

	void step(int by)
	{
		if (cands_.isEmpty())
			return;
		const int i = qBound(0, base_ + bias_ + by, int(cands_.size()) - 1);
		bias_ = i - base_;
		biasPoint_ = cursor_;
		update();
	}

	void save(const QRect &box)
	{
		if (box.isEmpty()) {
			finish(QString(), QStringLiteral("There is nothing to save there. Point at a picture, or drag a box."));
			return;
		}
		const QImage out = view_.copy(box);
		const QString path = newPinpointPath();
		if (!out.save(path, "PNG")) {
			finish(QString(), QStringLiteral("Couldn't save the picture to ") + path);
			return;
		}
		if (QClipboard *clip = QGuiApplication::clipboard())
			clip->setImage(out);
		finish(path, QString());
	}

	void finish(const QString &path, const QString &error)
	{
		if (finished_)
			return;
		finished_ = true;
		hide();
		close();
		if (done_)
			done_(path, error);
	}

	QImage view_;
	std::unique_ptr<PinSnap> snap_;
	QList<Candidate> cands_;
	QPoint cursor_;
	QPoint biasPoint_;
	int base_ = 0;
	int bias_ = 0;
	bool pressed_ = false;
	bool dragging_ = false;
	bool finished_ = false;
	QPointF pressPos_;
	QRect manual_;
	Done done_;
};

} // namespace

namespace hopcast {

void takePinpointScreenshot(std::function<void(const QString &path, const QString &error)> done)
{
	captureDesktopImage([done](const QImage &image, const QString &error) {
		if (image.isNull()) {
			done(QString(), error.isEmpty() ? QStringLiteral("Couldn't take a picture of the screen.") : error);
			return;
		}
		QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
		if (!screen)
			screen = QGuiApplication::primaryScreen();
		if (!screen) {
			done(QString(), QStringLiteral("No screen found."));
			return;
		}
		new PinpointOverlay(image, screen, done); // deletes itself when closed
	});
}

} // namespace hopcast
