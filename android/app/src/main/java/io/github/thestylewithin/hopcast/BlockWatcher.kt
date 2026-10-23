package io.github.thestylewithin.hopcast

import android.accessibilityservice.AccessibilityService
import android.content.Context
import android.provider.Settings
import android.view.accessibility.AccessibilityEvent
import android.view.inputmethod.InputMethodManager

/**
 * Notices which app is on screen. When it is on the block list, the picture goes black at once, on the stream and
 * in the recording. It reads no content and sends nothing anywhere; it only looks at the app's name.
 */
class BlockWatcher : AccessibilityService() {

  override fun onAccessibilityEvent(event: AccessibilityEvent?) {
    if (event == null || event.eventType != AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED) return
    val pkg = event.packageName?.toString() ?: return
    if (pkg == packageName || ignored(pkg)) return
    Studio.load(applicationContext)
    val blocked = Studio.blocked.contains(pkg)
    if (Live.foreground != pkg || Live.blocked != blocked) {
      Live.foreground = pkg
      Live.blocked = blocked
      CaptureService.instance?.onBlockChanged()
    }
  }

  override fun onInterrupt() {}

  /** The notification shade and keyboards also open windows; they say nothing about which app you're in. */
  private fun ignored(pkg: String): Boolean {
    if (pkg == "com.android.systemui") return true
    val keyboards = getSystemService(InputMethodManager::class.java)?.enabledInputMethodList?.map { it.packageName } ?: emptyList()
    return pkg in keyboards
  }

  companion object {
    fun enabled(context: Context): Boolean {
      val list = Settings.Secure.getString(context.contentResolver, Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES) ?: return false
      return list.split(':').any { it.startsWith(context.packageName + "/") && it.endsWith("BlockWatcher") }
    }
  }
}
