package io.github.thestylewithin.hopcast

import android.os.Handler
import android.os.Looper
import java.util.concurrent.CopyOnWriteArrayList

/** What the capture service is doing right now, so the screen, the bubble and the notification agree. */
object Live {
  /** Capture is running (streaming and/or recording). */
  @Volatile var running = false
  /** Permission was given; waiting for HopCast to leave the screen before the stream starts. */
  @Volatile var waiting = false
  /** The user hid their screen: viewers see black. */
  @Volatile var hidden = false
  @Volatile var micOn = false
  /** Streaming to the server (false while only recording, e.g. after a failed connection). */
  @Volatile var streaming = false
  @Volatile var startedMs = 0L
  /** HopCast's own screen is showing. It is never shown on the stream. */
  @Volatile var appVisible = false
  @Volatile var status = ""

  private val listeners = CopyOnWriteArrayList<() -> Unit>()
  private val main = Handler(Looper.getMainLooper())

  fun active() = running || waiting

  fun addListener(l: () -> Unit) { listeners.add(l) }
  fun removeListener(l: () -> Unit) { listeners.remove(l) }
  fun changed() { main.post { listeners.forEach { it() } } }

  fun reset() {
    running = false; waiting = false; hidden = false; micOn = false; streaming = false; startedMs = 0L
  }
}
