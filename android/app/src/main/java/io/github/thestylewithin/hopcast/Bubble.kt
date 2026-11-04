package io.github.thestylewithin.hopcast

import android.content.Context
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.LinearLayout
import android.widget.TextView

/**
 * The small floating circle that stays over other apps while HopCast captures. Tap it for the quick buttons
 * (hide screen, microphone, stop); drag it out of the way. It is marked secure so Android leaves it out of the
 * picture that is streamed and recorded.
 */
class Bubble(private val context: Context) {

  private val wm = context.getSystemService(WindowManager::class.java)
  private val main = Handler(Looper.getMainLooper())
  private val density = context.resources.displayMetrics.density
  private var root: LinearLayout? = null
  private var circle: TextView? = null
  private var tools: LinearLayout? = null
  private var hideButton: TextView? = null
  private var micButton: TextView? = null
  private var params: WindowManager.LayoutParams? = null

  private val ticker = object : Runnable {
    override fun run() {
      update()
      main.postDelayed(this, 1000)
    }
  }

  private fun dp(v: Int) = (v * density).toInt()

  private fun round(color: Int, strokeColor: Int? = null): GradientDrawable = GradientDrawable().apply {
    shape = GradientDrawable.OVAL
    setColor(color)
    if (strokeColor != null) setStroke(dp(2), strokeColor)
  }

  private fun button(label: String, color: Int, onClick: () -> Unit): TextView = TextView(context).apply {
    text = label
    setTextColor(Color.WHITE)
    textSize = 10f
    gravity = Gravity.CENTER
    background = round(color)
    layoutParams = LinearLayout.LayoutParams(dp(48), dp(48)).apply { topMargin = dp(6) }
    setOnClickListener { onClick() }
  }

  fun show() {
    if (root != null) return
    val layout = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER_HORIZONTAL }

    val c = TextView(context).apply {
      setTextColor(Color.WHITE)
      textSize = 10f
      gravity = Gravity.CENTER
      background = round(Color.parseColor("#1b1b1b"), Color.parseColor("#effc35"))
      layoutParams = LinearLayout.LayoutParams(dp(56), dp(56))
    }
    layout.addView(c)

    val t = LinearLayout(context).apply { orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER_HORIZONTAL; visibility = View.GONE }
    hideButton = button("Hide", Color.parseColor("#3a3a3a")) { CaptureService.send(context, CaptureService.ACTION_HIDE) }
    micButton = button("Mic", Color.parseColor("#3a3a3a")) { CaptureService.send(context, CaptureService.ACTION_MIC) }
    t.addView(hideButton)
    t.addView(micButton)
    if (Studio.scene().sources.any { it.kind == "camera" && it.text == "switch" && it.visible }) {
      t.addView(button("Flip", Color.parseColor("#3a3a3a")) { CaptureService.send(context, CaptureService.ACTION_FLIP) })
    }
    t.addView(button("Stop", Color.parseColor("#c34151")) { CaptureService.send(context, CaptureService.ACTION_STOP) })
    layout.addView(t)

    val lp = WindowManager.LayoutParams(
      WindowManager.LayoutParams.WRAP_CONTENT, WindowManager.LayoutParams.WRAP_CONTENT,
      WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
      WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS or WindowManager.LayoutParams.FLAG_SECURE,
      PixelFormat.TRANSLUCENT,
    ).apply {
      gravity = Gravity.TOP or Gravity.START
      x = dp(8)
      y = dp(160)
    }

    var downX = 0f
    var downY = 0f
    var startX = 0
    var startY = 0
    var moved = false
    c.setOnTouchListener { _, e ->
      when (e.action) {
        MotionEvent.ACTION_DOWN -> {
          downX = e.rawX; downY = e.rawY; startX = lp.x; startY = lp.y; moved = false; true
        }
        MotionEvent.ACTION_MOVE -> {
          val dx = (e.rawX - downX).toInt()
          val dy = (e.rawY - downY).toInt()
          if (Math.abs(dx) > dp(8) || Math.abs(dy) > dp(8)) moved = true
          if (moved) {
            lp.x = startX + dx
            lp.y = startY + dy
            wm.updateViewLayout(layout, lp)
          }
          true
        }
        MotionEvent.ACTION_UP -> {
          if (!moved) t.visibility = if (t.visibility == View.VISIBLE) View.GONE else View.VISIBLE
          true
        }
        else -> false
      }
    }

    try {
      wm.addView(layout, lp)
    } catch (e: Exception) {
      return // no permission to draw over other apps: HopCast still works from the notification
    }
    root = layout
    circle = c
    tools = t
    params = lp
    update()
    main.postDelayed(ticker, 1000)
  }

  fun update() {
    val c = circle ?: return
    val seconds = if (Live.startedMs > 0) ((System.currentTimeMillis() - Live.startedMs) / 1000).toInt() else 0
    val time = String.format("%02d:%02d", seconds / 60, seconds % 60)
    c.text = when {
      Live.waiting -> "WAIT"
      Live.running && Live.streaming -> "LIVE\n$time"
      Live.running -> "REC\n$time"
      else -> ""
    }
    hideButton?.text = if (Live.hidden) "Show" else "Hide"
    micButton?.text = if (Live.micOn) "Mic\nON" else "Mic\noff"
  }

  fun hide() {
    main.removeCallbacks(ticker)
    root?.let { try { wm.removeView(it) } catch (e: Exception) { } }
    root = null
  }
}
