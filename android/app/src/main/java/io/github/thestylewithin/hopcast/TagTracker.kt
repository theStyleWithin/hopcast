package io.github.thestylewithin.hopcast

import android.graphics.Bitmap
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock

/**
 * Finds the tagged apps (their icon and name) in small grey pictures of your screen, so black boxes can follow them
 * wherever they are, and notices when the screen is sliding around (you swipe between home pages).
 */
class TagTracker(private val onResult: (List<Hit>, Boolean) -> Unit) {

  /** Where a tagged app is: left, top, width, height, as percents of the picture. */
  class Hit(val tag: Studio.Tag, val x: Float, val y: Float, val w: Float, val h: Float)

  private class Template(val tag: Studio.Tag, val w: Int, val h: Int, val px: ByteArray, var lastX: Int = -1, var lastY: Int = -1, var seenMs: Long = 0)

  private val thread = HandlerThread("hopcast-tracker").apply { start() }
  private val handler = Handler(thread.looper)
  private var templates = listOf<Template>()
  private var builtFor = 0
  private var previous: ByteArray? = null
  private var movingUntil = 0L
  @Volatile private var busy = false
  @Volatile private var stale = true

  /** Call after tags were added or removed. */
  fun reload() { stale = true }

  fun stop() {
    handler.removeCallbacksAndMessages(null)
    thread.quitSafely()
  }

  fun onGrey(grey: ByteArray, gw: Int, gh: Int, step: Int) {
    if (busy) return
    busy = true
    handler.post {
      try { process(grey, gw, gh, step) } finally { busy = false }
    }
  }

  private fun build(gw: Int, step: Int) {
    val frameW = gw * step
    templates = Studio.tags.mapNotNull { tag ->
      val bitmap = Studio.decode(tag.file, 4096) ?: return@mapNotNull null
      val scale = if (tag.frameW > 0) frameW.toFloat() / tag.frameW else 1f
      val tw = Math.max(4, Math.round(bitmap.width * scale / step))
      val th = Math.max(4, Math.round(bitmap.height * scale / step))
      val small = Bitmap.createScaledBitmap(bitmap, tw, th, true)
      val px = ByteArray(tw * th)
      val ints = IntArray(tw * th)
      small.getPixels(ints, 0, tw, 0, 0, tw, th)
      for (i in ints.indices) {
        val c = ints[i]
        px[i] = ((((c shr 16) and 0xff) * 77 + ((c shr 8) and 0xff) * 150 + (c and 0xff) * 29) shr 8).toByte()
      }
      Template(tag, tw, th, px)
    }
    builtFor = gw
    stale = false
  }

  private fun process(grey: ByteArray, gw: Int, gh: Int, step: Int) {
    if (stale || builtFor != gw) build(gw, step)
    val now = SystemClock.uptimeMillis()

    // is the whole screen sliding?
    previous?.let { prev ->
      if (prev.size == grey.size) {
        var changed = 0
        var i = 0
        while (i < grey.size) {
          if (Math.abs((grey[i].toInt() and 0xff) - (prev[i].toInt() and 0xff)) > 24) changed++
          i += 3
        }
        if (changed * 3 > grey.size * 0.30) movingUntil = now + 350
      }
    }
    previous = grey

    val hits = mutableListOf<Hit>()
    val frameW = gw * step
    val frameH = gh * step
    for (t in templates) {
      if (gw < t.w || gh < t.h) continue
      var pos = if (t.lastX >= 0) find(grey, gw, gh, t, Math.max(0, t.lastX - 10), Math.max(0, t.lastY - 10), Math.min(gw - t.w, t.lastX + 10), Math.min(gh - t.h, t.lastY + 10), 1) else null
      if (pos == null) pos = find(grey, gw, gh, t, 0, 0, gw - t.w, gh - t.h, 2)
      if (pos != null) {
        t.lastX = pos.first; t.lastY = pos.second; t.seenMs = now
      } else if (now - t.seenMs > 300) {
        t.lastX = -1
      }
      if (t.lastX >= 0 && now - t.seenMs <= 300) {
        // a little extra around it, so the edges are covered too
        val padX = t.w * 0.12f
        val padY = t.h * 0.12f
        hits.add(Hit(t.tag, (t.lastX - padX) * step / frameW * 100f, (t.lastY - padY) * step / frameH * 100f, (t.w + padX * 2) * step / frameW * 100f, (t.h + padY * 2) * step / frameH * 100f))
      }
    }
    onResult(hits, now < movingUntil)
  }

  /** Slides the template over the picture and returns the best spot, if it looks enough like the tagged app. */
  private fun find(g: ByteArray, gw: Int, gh: Int, t: Template, x0: Int, y0: Int, x1: Int, y1: Int, posStep: Int): Pair<Int, Int>? {
    var best = Long.MAX_VALUE
    var bx = -1
    var by = -1
    val pixStep = 2
    var y = y0
    while (y <= y1) {
      var x = x0
      while (x <= x1) {
        var sad = 0L
        var ty = 0
        while (ty < t.h) {
          val row = (y + ty) * gw + x
          val trow = ty * t.w
          var tx = 0
          while (tx < t.w) {
            sad += Math.abs((g[row + tx].toInt() and 0xff) - (t.px[trow + tx].toInt() and 0xff))
            tx += pixStep
          }
          ty += pixStep
          if (sad >= best) break
        }
        if (sad < best) { best = sad; bx = x; by = y }
        x += posStep
      }
      y += posStep
    }
    if (bx < 0) return null
    val count = ((t.w + 1) / pixStep) * ((t.h + 1) / pixStep)
    return if (best.toFloat() / Math.max(1, count) < THRESHOLD) Pair(bx, by) else null
  }

  private companion object {
    /** How different (grey levels per pixel, on average) a spot may be and still count as the tagged app. */
    const val THRESHOLD = 14f
  }
}
