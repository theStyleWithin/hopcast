package io.github.thestylewithin.hopcast

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Shader
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View

/**
 * Shows the stream's layout: your sources over a placeholder for your screen (HopCast can't show your real
 * screen inside itself). Drag a source to move it, pinch to resize it.
 */
class PreviewView(context: Context) : View(context) {

  var portrait = false
    set(value) { field = value; requestLayout(); invalidate() }
  var selected: Studio.Source? = null
    set(value) { field = value; invalidate() }
  var onSelect: (Studio.Source?) -> Unit = {}
  var onChanged: () -> Unit = {}

  private val density = resources.displayMetrics.density
  private val canvasRect = RectF()
  private val bitmaps = HashMap<String, Bitmap?>()
  private val back = Paint()
  private val hint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#77ffffff"); textAlign = Paint.Align.CENTER; textSize = 12f * density; letterSpacing = 0.1f }
  private val outline = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#effc35"); style = Paint.Style.STROKE; strokeWidth = 2f * density; pathEffect = DashPathEffect(floatArrayOf(10f, 8f), 0f) }
  private val dot = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#effc35") }
  private val bitmapPaint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG)

  private var dragging: Studio.Source? = null
  private var lastX = 0f
  private var lastY = 0f
  private var moved = false

  private val scaler = ScaleGestureDetector(context, object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
    override fun onScale(detector: ScaleGestureDetector): Boolean {
      val s = selected ?: return false
      s.w = (s.w * detector.scaleFactor).coerceIn(3f, 100f)
      invalidate()
      return true
    }
  })

  override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
    val w = MeasureSpec.getSize(widthMeasureSpec)
    val h = if (portrait) (300 * density).toInt() else (w - (20 * density).toInt()) * 9 / 16 + (16 * density).toInt()
    setMeasuredDimension(w, h)
  }

  private fun layoutCanvas() {
    val pad = 8f * density
    val availW = width - pad * 2
    val availH = height - pad * 2
    val aspect = if (portrait) 9f / 16f else 16f / 9f
    var w = availW
    var h = w / aspect
    if (h > availH) { h = availH; w = h * aspect }
    val left = (width - w) / 2f
    val top = (height - h) / 2f
    canvasRect.set(left, top, left + w, top + h)
  }

  private fun bitmapFor(s: Studio.Source): Bitmap? = bitmaps.getOrPut(s.file) { Studio.decode(s.file) }

  private fun rectOf(s: Studio.Source): RectF {
    val w = s.w / 100f * canvasRect.width()
    val h = w / s.aspect.coerceAtLeast(0.05f)
    val left = canvasRect.left + s.x / 100f * canvasRect.width()
    val top = canvasRect.top + s.y / 100f * canvasRect.height()
    return RectF(left, top, left + w, top + h)
  }

  override fun onDraw(canvas: Canvas) {
    layoutCanvas()
    canvas.drawColor(Color.parseColor("#151515"))
    back.shader = LinearGradient(canvasRect.left, canvasRect.top, canvasRect.right, canvasRect.bottom, Color.parseColor("#12243b"), Color.parseColor("#3d2535"), Shader.TileMode.CLAMP)
    canvas.drawRect(canvasRect, back)
    canvas.drawText("YOUR SCREEN", canvasRect.centerX(), canvasRect.centerY(), hint)
    canvas.save()
    canvas.clipRect(canvasRect)
    Studio.scene().sources.filter { it.visible }.forEach { s ->
      bitmapFor(s)?.let { canvas.drawBitmap(it, null, rectOf(s), bitmapPaint) }
    }
    canvas.restore()
    selected?.let { s ->
      val r = rectOf(s)
      canvas.drawRect(r, outline)
      val rad = 4.5f * density
      listOf(r.left to r.top, r.right to r.top, r.left to r.bottom, r.right to r.bottom).forEach { (x, y) -> canvas.drawCircle(x, y, rad, dot) }
    }
  }

  private fun hit(x: Float, y: Float): Studio.Source? =
    Studio.scene().sources.filter { it.visible }.lastOrNull { rectOf(it).contains(x, y) }

  override fun onTouchEvent(e: MotionEvent): Boolean {
    scaler.onTouchEvent(e)
    when (e.actionMasked) {
      MotionEvent.ACTION_DOWN -> {
        parent?.requestDisallowInterceptTouchEvent(true)
        lastX = e.x; lastY = e.y; moved = false
        dragging = hit(e.x, e.y)
        if (dragging != null && dragging !== selected) { selected = dragging; onSelect(dragging) }
      }
      MotionEvent.ACTION_MOVE -> {
        if (!scaler.isInProgress && e.pointerCount == 1) {
          val s = dragging
          if (s != null) {
            val dx = e.x - lastX
            val dy = e.y - lastY
            if (Math.abs(dx) + Math.abs(dy) > 2f) moved = true
            s.x = (s.x + dx / canvasRect.width() * 100f).coerceIn(-s.w + 5f, 95f)
            s.y = (s.y + dy / canvasRect.height() * 100f).coerceIn(-20f, 95f)
            invalidate()
          }
        }
        lastX = e.x; lastY = e.y
      }
      MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
        if (e.actionMasked == MotionEvent.ACTION_UP && dragging == null && !moved) {
          selected = null
          onSelect(null)
        }
        dragging = null
        Studio.save()
        onChanged()
      }
    }
    return true
  }

  /** Forget the cached pictures after a source is added, changed or removed. */
  fun reload() { bitmaps.clear(); invalidate() }
}
