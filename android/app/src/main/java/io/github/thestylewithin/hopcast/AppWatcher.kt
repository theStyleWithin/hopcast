package io.github.thestylewithin.hopcast

import android.app.AppOpsManager
import android.app.usage.UsageEvents
import android.app.usage.UsageStatsManager
import android.content.Context
import android.os.Handler
import android.os.HandlerThread
import android.os.Process
import android.view.inputmethod.InputMethodManager

/**
 * Notices which app is on screen, using Android's "usage access" (the same thing parental-control apps use). When an app
 * on the block list comes to the front, the picture goes black at once, on the stream and in the recording. It only
 * ever looks at the app's name; it reads nothing inside the app and sends nothing anywhere.
 */
class AppWatcher(private val context: Context) {

  private val usage = context.getSystemService(UsageStatsManager::class.java)
  private val thread = HandlerThread("hopcast-app-watcher").apply { start() }
  private val handler = Handler(thread.looper)
  private var lastQuery = 0L
  @Volatile private var running = false
  private var keyboards: Set<String> = emptySet()

  private val poll = object : Runnable {
    override fun run() {
      if (!running) return
      look()
      handler.postDelayed(this, POLL_MS)
    }
  }

  fun start() {
    if (running) return
    running = true
    keyboards = context.getSystemService(InputMethodManager::class.java)?.enabledInputMethodList?.map { it.packageName }?.toSet() ?: emptySet()
    lastQuery = 0L
    handler.post(poll)
  }

  fun stop() {
    running = false
    handler.removeCallbacksAndMessages(null)
    thread.quitSafely()
  }

  /** Reads what Android recorded since the last look and takes the app that came to the front most recently. */
  private fun look() {
    if (!allowed(context)) return
    val now = System.currentTimeMillis()
    val from = if (lastQuery == 0L) now - 60_000 else lastQuery - 300
    var front: String? = null
    try {
      val events = usage.queryEvents(from, now)
      val e = UsageEvents.Event()
      while (events.hasNextEvent()) {
        events.getNextEvent(e)
        if (e.eventType == UsageEvents.Event.ACTIVITY_RESUMED) front = e.packageName
      }
    } catch (ex: Exception) {
      return
    }
    lastQuery = now
    val pkg = front ?: return
    if (pkg == context.packageName || pkg == "com.android.systemui" || pkg in keyboards) return
    val game = Live.gamePackage
    val blocked = if (game.isNotEmpty()) pkg != game else Studio.blockedFast.contains(pkg)
    if (Live.foreground != pkg || Live.blocked != blocked) {
      Live.foreground = pkg
      Live.blocked = blocked
      Handler(context.mainLooper).post { CaptureService.instance?.onBlockChanged() }
    }
  }

  companion object {
    private const val POLL_MS = 120L

    /** The app you were in just before opening HopCast (a likely game), as its package name, or null. */
    fun lastAppBeforeUs(context: Context): String? {
      if (!allowed(context)) return null
      return try {
        val usage = context.getSystemService(UsageStatsManager::class.java)
        val keyboards = context.getSystemService(InputMethodManager::class.java)?.enabledInputMethodList?.map { it.packageName }?.toSet() ?: emptySet()
        val launcher = android.content.Intent(android.content.Intent.ACTION_MAIN).addCategory(android.content.Intent.CATEGORY_HOME)
        @Suppress("DEPRECATION")
        val homes = context.packageManager.queryIntentActivities(launcher, 0).map { it.activityInfo.packageName }.toSet()
        val now = System.currentTimeMillis()
        val events = usage.queryEvents(now - 6 * 3600_000L, now)
        val e = UsageEvents.Event()
        var last: String? = null
        while (events.hasNextEvent()) {
          events.getNextEvent(e)
          if (e.eventType == UsageEvents.Event.ACTIVITY_RESUMED) {
            val p = e.packageName
            if (p != context.packageName && p != "com.android.systemui" && p !in keyboards && p !in homes) last = p
          }
        }
        last
      } catch (ex: Exception) {
        null
      }
    }

    /** Whether the user has switched on "usage access" for HopCast. */
    fun allowed(context: Context): Boolean {
      val ops = context.getSystemService(AppOpsManager::class.java)
      return ops.unsafeCheckOpNoThrow(AppOpsManager.OPSTR_GET_USAGE_STATS, Process.myUid(), context.packageName) == AppOpsManager.MODE_ALLOWED
    }
  }
}
