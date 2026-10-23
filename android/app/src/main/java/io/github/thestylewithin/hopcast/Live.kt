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
  /** Recording only, not streaming (the Rec button). */
  @Volatile var recordOnly = false
  /** The live-preview security check is running: your screen shows as normal and nothing goes out. */
  @Volatile var checking = false
  /** The home screen is sliding around (a swipe), so the picture is black for a moment. */
  @Volatile var moving = false
  @Volatile var startedMs = 0L
  /** HopCast's own screen is showing. It is never shown on the stream. */
  @Volatile var appVisible = false
  @Volatile var status = ""
  /** The app on screen right now, as the block-list watcher last saw it. */
  @Volatile var foreground = ""
  /** An app on the block list is on screen: viewers and the recording see black. */
  @Volatile var blocked = false
  /** When set, only this app is ever shown on the stream and in the recording (the "Game" source). */
  @Volatile var gamePackage = ""
  /** The scene shows your screen (a Home screen or Game source). Without one, the picture is black behind the other sources. */
  @Volatile var hasBase = true
  /** The block list has apps but the watcher is switched off, so the picture stays black to be safe. */
  @Volatile var watcherDown = false

  private val listeners = CopyOnWriteArrayList<() -> Unit>()
  private val main = Handler(Looper.getMainLooper())

  fun active() = running || waiting

  fun addListener(l: () -> Unit) { listeners.add(l) }
  fun removeListener(l: () -> Unit) { listeners.remove(l) }
  fun changed() { main.post { listeners.forEach { it() } } }

  fun reset() {
    running = false; waiting = false; hidden = false; micOn = false; streaming = false; recordOnly = false; checking = false; moving = false; startedMs = 0L
    blocked = false; watcherDown = false; foreground = ""; gamePackage = ""
  }
}
