package io.github.thestylewithin.hopcast

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.DashPathEffect
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import java.io.File
import java.io.FileOutputStream

/**
 * The live-preview security check. Your home screen shows as normal and nothing goes out. Tap "Tag an app", HopCast
 * freezes a picture of your screen, you put the square over an app's icon and name and save it. HopCast then keeps a
 * black box over that app wherever it appears. Scroll to another page and tag more.
 */
class CheckOverlay(private val context: Context, private val service: CaptureService) {

  private val wm = context.getSystemService(WindowManager::class.java)
  private val density = context.resources.displayMetrics.density
  private val handler = Handler(Looper.getMainLooper())
  private var banner: LinearLayout? = null
  private var count: TextView? = null
  private var full: FrameLayout? = null

  private fun dp(v: Int) = (v * density).toInt()

  private fun params(fullScreen: Boolean) = WindowManager.LayoutParams(
    WindowManager.LayoutParams.MATCH_PARENT,
    if (fullScreen) WindowManager.LayoutParams.MATCH_PARENT else WindowManager.LayoutParams.WRAP_CONTENT,
    WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
    WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
      WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS or WindowManager.LayoutParams.FLAG_SECURE,
    PixelFormat.TRANSLUCENT,
  ).apply { gravity = Gravity.TOP }

  private fun text(value: String, size: Float, color: Int, bold: Boolean = false) = TextView(context).apply {
    text = value; textSize = size; setTextColor(color)
    if (bold) typeface = Typeface.DEFAULT_BOLD
  }

  private fun button(label: String, fill: String, onClick: () -> Unit) = Button(context).apply {
    text = label; isAllCaps = false; textSize = 12f; setTextColor(if (fill == "#effc35") Color.BLACK else Color.WHITE)
    background = GradientDrawable().apply { setColor(Color.parseColor(fill)); cornerRadius = dp(4).toFloat() }
    minHeight = 0; minimumHeight = 0; setPadding(dp(10), dp(6), dp(10), dp(6))
    layoutParams = LinearLayout.LayoutParams(0, dp(36), 1f).apply { leftMargin = dp(4); rightMargin = dp(4) }
    setOnClickListener { onClick() }
  }

  fun show() {
    if (banner != null) return
    val b = LinearLayout(context).apply {
      orientation = LinearLayout.VERTICAL
      setPadding(dp(12), dp(40), dp(12), dp(10))
      setBackgroundColor(Color.parseColor("#e61b1b1b"))
    }
    b.addView(text("Live-preview security check", 14f, Color.parseColor("#effc35"), true))
    b.addView(text("Nothing is going out. Scroll to the page you want, then tag each app you don't want shown.", 12f, Color.WHITE))
    count = text("", 12f, Color.parseColor("#bdbdbd")).apply { setPadding(0, dp(4), 0, dp(6)) }
    b.addView(count)
    val row = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL }
    row.addView(button("Tag an app", "#effc35") { beginTag() })
    row.addView(button("What viewers see", "#3a3a3a") { showViewerPicture() })
    row.addView(button("Done", "#c34151") { CaptureService.send(context, CaptureService.ACTION_STOP) })
    b.addView(row)
    banner = b
    updateCount()
    try { wm.addView(b, params(false)) } catch (e: Exception) { banner = null }
  }

  fun hide() {
    handler.removeCallbacksAndMessages(null)
    banner?.let { try { wm.removeView(it) } catch (e: Exception) { } }
    banner = null
    closeFull()
  }

  private fun updateCount() {
    val n = Studio.tags.size
    count?.text = if (n == 0) "No apps tagged yet." else "$n app${if (n == 1) "" else "s"} tagged."
  }

  private fun closeFull() {
    full?.let { try { wm.removeView(it) } catch (e: Exception) { } }
    full = null
  }

  /** Freezes a picture of the screen (with this banner out of the way) and lets you mark an app on it. */
  private fun beginTag() {
    val probe = service.probe()
    if (probe == null) { Toast.makeText(context, "Not ready yet, try again in a moment", Toast.LENGTH_SHORT).show(); return }
    banner?.visibility = View.INVISIBLE
    handler.postDelayed({
      probe.requestSnapshot { bitmap ->
        banner?.visibility = View.VISIBLE
        showEditor(bitmap)
      }
    }, 250)
  }

  private fun showEditor(bitmap: Bitmap) {
    closeFull()
    val view = FrozenEditorView(context, bitmap)
    val root = FrameLayout(context).apply { setBackgroundColor(Color.BLACK) }
    root.addView(view, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
    val top = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL; setBackgroundColor(Color.parseColor("#cc000000")); setPadding(dp(12), dp(40), dp(12), dp(8)) }
    top.addView(text("Tap the app's icon, then size the square so it covers the icon and its name.", 13f, Color.WHITE))
    root.addView(top, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP))
    val bottom = LinearLayout(context).apply { orientation = LinearLayout.HORIZONTAL; setBackgroundColor(Color.parseColor("#cc000000")); setPadding(dp(12), dp(8), dp(12), dp(24)) }
    bottom.addView(button("Smaller", "#3a3a3a") { view.scaleBox(0.9f) })
    bottom.addView(button("Bigger", "#3a3a3a") { view.scaleBox(1.1f) })
    bottom.addView(button("Cancel", "#3a3a3a") { closeFull() })
    bottom.addView(button("Save", "#effc35") { saveTag(view, bitmap) })
    root.addView(bottom, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM))
    full = root
    try { wm.addView(root, params(true)) } catch (e: Exception) { full = null }
  }

  private fun saveTag(view: FrozenEditorView, bitmap: Bitmap) {
    val raw = view.boxInBitmap()
    if (raw.width() < 12 || raw.height() < 12) { Toast.makeText(context, "Tap the app first", Toast.LENGTH_SHORT).show(); return }
    // snap to the tracker's grid, so the saved picture lines up with what it sees later
    val g = ProbeFilterRender.STEP
    val r = Rect(
      raw.left / g * g, raw.top / g * g,
      Math.min(bitmap.width / g * g, (raw.right + g - 1) / g * g), Math.min(bitmap.height / g * g, (raw.bottom + g - 1) / g * g),
    )
    if (r.width() < 12 || r.height() < 12) { Toast.makeText(context, "Tap the app first", Toast.LENGTH_SHORT).show(); return }
    val crop = Bitmap.createBitmap(bitmap, r.left, r.top, r.width(), r.height())
    if (!lookDistinct(crop)) {
      Toast.makeText(context, "That spot looks too plain to follow. Make the square bigger so it includes the icon and its name.", Toast.LENGTH_LONG).show()
      return
    }
    val id = Studio.newId()
    val file = File(Studio.filesDir(context, "tags"), "$id.png")
    try {
      FileOutputStream(file).use { crop.compress(Bitmap.CompressFormat.PNG, 100, it) }
    } catch (e: Exception) {
      Toast.makeText(context, "Couldn't save the tag", Toast.LENGTH_SHORT).show()
      return
    }
    Studio.tags.add(Studio.Tag(id, "App " + (Studio.tags.size + 1), file.path, crop.width, crop.height, bitmap.width, bitmap.height))
    Studio.save()
    service.onTagsChanged()
    updateCount()
    closeFull()
    Toast.makeText(context, "Tagged. Scroll to another page to tag more.", Toast.LENGTH_SHORT).show()
  }

  /** A picture with almost no detail can't be told apart from a plain background, so it can't be followed. */
  private fun lookDistinct(bitmap: Bitmap): Boolean {
    val small = Bitmap.createScaledBitmap(bitmap, 24, 24, true)
    val ints = IntArray(24 * 24)
    small.getPixels(ints, 0, 24, 0, 0, 24, 24)
    val greys = ints.map { (((it shr 16) and 0xff) * 77 + ((it shr 8) and 0xff) * 150 + (it and 0xff) * 29) shr 8 }
    val mean = greys.average()
    val deviation = Math.sqrt(greys.sumOf { (it - mean) * (it - mean) } / greys.size)
    return deviation > 10.0
  }

  /** Shows the picture viewers would get right now, black boxes included. */
  private fun showViewerPicture() {
    service.viewerPicture { bitmap ->
      handler.post {
        closeFull()
        val root = FrameLayout(context).apply { setBackgroundColor(Color.BLACK); setOnClickListener { closeFull() } }
        root.addView(ImageView(context).apply { setImageBitmap(bitmap); scaleType = ImageView.ScaleType.FIT_CENTER }, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        val total = Studio.tags.size
        val covered = service.tagsFollowed()
        val line = when {
          total == 0 -> "No apps tagged yet."
          else -> "$covered black box${if (covered == 1) "" else "es"} on this page, $total app${if (total == 1) "" else "s"} tagged in all (the rest are on other pages, or HopCast can't find them)."
        }
        root.addView(text("What viewers would see. Tap to close.\n$line", 13f, Color.WHITE).apply { setBackgroundColor(Color.parseColor("#cc000000")); setPadding(dp(12), dp(40), dp(12), dp(8)) },
          FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP))
        full = root
        try { wm.addView(root, params(true)) } catch (e: Exception) { full = null }
      }
    }
  }

  /** The frozen screen picture with a square you place over an app. */
  private class FrozenEditorView(context: Context, private val bitmap: Bitmap) : View(context) {
    private val fit = RectF()
    private var cx = -1f
    private var cy = -1f
    private var boxW = 0.17f // of the picture's width
    private val ratio = 1.3f // height / width: an icon with its name under it
    private val dim = Paint().apply { color = Color.parseColor("#88000000") }
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#effc35"); style = Paint.Style.STROKE; strokeWidth = 4f; pathEffect = DashPathEffect(floatArrayOf(14f, 10f), 0f) }
    private val picture = Paint(Paint.FILTER_BITMAP_FLAG)

    private fun layoutFit() {
      val scale = Math.min(width.toFloat() / bitmap.width, height.toFloat() / bitmap.height)
      val w = bitmap.width * scale
      val h = bitmap.height * scale
      fit.set((width - w) / 2f, (height - h) / 2f, (width + w) / 2f, (height + h) / 2f)
    }

    private fun boxRect(): RectF {
      val w = boxW * fit.width()
      val h = w * ratio
      return RectF(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)
    }

    fun scaleBox(f: Float) { boxW = (boxW * f).coerceIn(0.05f, 0.6f); invalidate() }

    fun boxInBitmap(): Rect {
      if (cx < 0) return Rect()
      val r = boxRect()
      val sx = bitmap.width / fit.width()
      val sy = bitmap.height / fit.height()
      val left = Math.max(0, ((r.left - fit.left) * sx).toInt())
      val top = Math.max(0, ((r.top - fit.top) * sy).toInt())
      val right = Math.min(bitmap.width, ((r.right - fit.left) * sx).toInt())
      val bottom = Math.min(bitmap.height, ((r.bottom - fit.top) * sy).toInt())
      return Rect(left, top, Math.max(left, right), Math.max(top, bottom))
    }

    override fun onDraw(canvas: Canvas) {
      layoutFit()
      canvas.drawColor(Color.BLACK)
      canvas.drawBitmap(bitmap, null, fit, picture)
      if (cx >= 0) {
        val r = boxRect()
        canvas.drawRect(r, stroke)
      }
    }

    override fun onTouchEvent(e: MotionEvent): Boolean {
      if (e.actionMasked == MotionEvent.ACTION_DOWN || e.actionMasked == MotionEvent.ACTION_MOVE) {
        layoutFit()
        if (fit.contains(e.x, e.y)) { cx = e.x; cy = e.y; invalidate() }
      }
      return true
    }
  }
}
