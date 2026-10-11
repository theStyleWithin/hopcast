/* HopCast (Linux): placeholder draw-pen backend.
 * Windows draws strokes with Direct2D (windows/), macOS with Core Graphics (mac/).
 * Until a Linux renderer exists (e.g. QPainter into a texture via the libobs
 * graphics API), this backend accepts input but draws nothing, so the rest of
 * the app runs normally. Original code, GPL-2.0-or-later. */
#pragma once
#include "../PLSDrawPenInterface.h"

class PLSDrawPenLinux : public PLSDrawPenInterface {
public:
	void beginDraw(unsigned int, unsigned int, unsigned int, PointF) override {}
	void beginDraw(PointF) override {}
	void moveTo(PointF) override {}
	void endDraw(PointF) override {}
	void eraseOn(PointF) override {}
	void undo() override {}
	void redo() override {}
	void clear() override {}
	void resize(float, float) override {}
	void setVisible(bool visible) override { m_visible = visible; }
	bool visible() override { return m_visible; }
	bool undoEmpty() override { return true; }
	bool redoEmpty() override { return true; }
	void drawPens() override {}
	void setCallback(void *context, DrawPenCallBacak cb) override
	{
		if (cb)
			cb(context, true, true);
	}

private:
	bool m_visible = true;
};
