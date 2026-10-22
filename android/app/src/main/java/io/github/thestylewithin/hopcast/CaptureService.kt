package io.github.thestylewithin.hopcast

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.ContentValues
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.graphics.Point
import android.graphics.drawable.Icon
import android.hardware.display.DisplayManager
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Environment
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.provider.MediaStore
import android.provider.Settings
import android.view.Display
import android.widget.Toast
import com.pedro.common.ConnectChecker
import com.pedro.encoder.input.sources.audio.AudioSource
import com.pedro.encoder.input.sources.audio.InternalAudioSource
import com.pedro.encoder.input.sources.audio.MicrophoneSource
import com.pedro.encoder.input.sources.audio.MixAudioSource
import com.pedro.encoder.input.sources.audio.NoAudioSource
import com.pedro.encoder.input.sources.video.ScreenSource
import com.pedro.library.generic.GenericStream
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Captures the screen and sends it to the server while HopCast sits in the background, and always
 * records the same picture to an MP4 on the phone. HopCast's own screen is never shown on the stream.
 */
class CaptureService : Service() {

  private class Config(
    val url: String, val key: String, val sound: Boolean,
    val quality: Int, val shape: Int, val bubble: Boolean,
  )

  private val main = Handler(Looper.getMainLooper())
  private var config: Config? = null
  private var projection: MediaProjection? = null
  private var stream: GenericStream? = null
  private var bubble: Bubble? = null
  private var recordFile: File? = null
  private var finishing = false

  private val beginRunnable = Runnable { if (Live.waiting && !Live.appVisible) beginCapture() }

  private val checker = object : ConnectChecker {
    override fun onConnectionStarted(url: String) {}
    override fun onConnectionSuccess() {
      main.post { Live.streaming = true; Live.status = ""; refresh() }
    }
    override fun onConnectionFailed(reason: String) { main.post { streamStopped("Couldn't stream: ${clean(reason)}") } }
    override fun onNewBitrate(bitrate: Long) {}
    override fun onDisconnect() {
      main.post { if (Live.streaming) streamStopped("The stream was disconnected") }
    }
    override fun onAuthError() { main.post { streamStopped("The server didn't accept the login") } }
    override fun onAuthSuccess() {}
  }

  override fun onCreate() {
    super.onCreate()
    val channel = NotificationChannel(CHANNEL, "HopCast is capturing", NotificationManager.IMPORTANCE_LOW)
    getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
  }

  override fun onBind(intent: Intent?): IBinder? = null

  override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
    when (intent?.action) {
      ACTION_START -> start(intent)
      ACTION_STOP -> finish()
      ACTION_HIDE -> { Live.hidden = !Live.hidden; applyMute(); refresh() }
      ACTION_MIC -> toggleMic()
      else -> if (!Live.active() && !finishing) stopSelf()
    }
    return START_NOT_STICKY
  }

  override fun onDestroy() {
    if (instance === this) instance = null
    super.onDestroy()
  }

  private fun start(intent: Intent) {
    if (Live.active() || finishing) {
      if (finishing) toast("HopCast is still saving the last recording")
      return
    }
    config = Config(
      url = intent.getStringExtra("url").orEmpty(),
      key = intent.getStringExtra("key").orEmpty(),
      sound = intent.getBooleanExtra("sound", true),
      quality = intent.getIntExtra("quality", 0),
      shape = intent.getIntExtra("shape", 0),
      bubble = intent.getBooleanExtra("bubble", true),
    )
    Live.reset()
    Live.micOn = intent.getBooleanExtra("mic", false)
    Live.status = ""
    instance = this

    var type = ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION
    if (checkSelfPermission(android.Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
      type = type or ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
    }
    startForeground(NOTIF_ID, notification("Open your game to start"), type)

    @Suppress("DEPRECATION")
    val data = intent.getParcelableExtra<Intent>("data")
    val proj = try {
      getSystemService(MediaProjectionManager::class.java).getMediaProjection(intent.getIntExtra("resultCode", 0), data!!)
    } catch (e: Exception) {
      null
    }
    if (proj == null) {
      Live.status = "Couldn't start screen capture"
      finish()
      return
    }
    proj.registerCallback(object : MediaProjection.Callback() {
      override fun onStop() { main.post { finish() } }
    }, main)
    projection = proj

    Live.waiting = true
    config?.let { cfg ->
      if (cfg.bubble && Settings.canDrawOverlays(this)) bubble = Bubble(this).also { it.show() }
    }
    refresh()
    if (!Live.appVisible) main.postDelayed(beginRunnable, SETTLE_MS)
  }

  /** HopCast's own screen came into view or left it. */
  fun onAppVisibility(visible: Boolean) {
    Live.appVisible = visible
    if (Live.waiting) {
      main.removeCallbacks(beginRunnable)
      if (!visible) main.postDelayed(beginRunnable, SETTLE_MS) // give the game a moment to open and settle its screen shape
    }
    applyMute()
  }

  /** Starts the stream and the recording. Runs once HopCast is out of view so the screen shape matches the game. */
  private fun beginCapture() {
    val cfg = config ?: return
    val proj = projection ?: return
    val portrait = when (cfg.shape) { 1 -> true; 2 -> false; else -> displayIsPortrait() }
    val longSide = if (cfg.quality == 1) 1920 else 1280
    val shortSide = if (cfg.quality == 1) 1080 else 720
    val fps = if (cfg.quality == 2) 60 else 30
    val bitrate = when (cfg.quality) { 1 -> 6_000_000; 2 -> 5_000_000; else -> 3_500_000 }
    try {
      val s = GenericStream(applicationContext, checker, ScreenSource(applicationContext, proj), audioFor(proj, cfg.sound, Live.micOn))
      // Screen capture only produces frames when the picture changes; this keeps the frame rate steady.
      s.getGlInterface().setForceRender(true, fps)
      val ready = s.prepareVideo(longSide, shortSide, bitrate, fps = fps, rotation = if (portrait) 90 else 0) &&
        s.prepareAudio(48000, true, 128_000)
      if (!ready) {
        Live.status = "This phone couldn't set up the video encoder"
        finish()
        return
      }
      stream = s
      applyMute()
      val file = newRecordFile()
      recordFile = file
      s.startRecord(file.absolutePath) { }
      Live.running = true
      Live.waiting = false
      Live.startedMs = System.currentTimeMillis()
      Live.streaming = false
      s.startStream(endpoint(cfg))
      refresh()
    } catch (e: Exception) {
      Live.status = "Couldn't start: ${clean(e.message ?: e.javaClass.simpleName)}"
      finish()
    }
  }

  private fun streamStopped(message: String) {
    Live.status = message
    Live.streaming = false
    try { stream?.takeIf { it.isStreaming }?.stopStream() } catch (e: Exception) { }
    toast(if (recordFile != null) "$message. Still recording to this phone." else message)
    refresh()
  }

  private fun toggleMic() {
    val proj = projection ?: return
    val want = !Live.micOn
    if (want && checkSelfPermission(android.Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
      toast("Open HopCast and switch Microphone on first, so Android can ask you")
      return
    }
    try {
      stream?.changeAudioSource(audioFor(proj, config?.sound ?: true, want))
    } catch (e: Exception) {
      toast("Couldn't switch the microphone")
      return
    }
    Live.micOn = want
    refresh()
  }

  private fun audioFor(proj: MediaProjection, sound: Boolean, mic: Boolean): AudioSource = when {
    sound && mic -> MixAudioSource(proj)
    sound -> InternalAudioSource(proj)
    mic -> MicrophoneSource()
    else -> NoAudioSource()
  }

  /** The picture goes black while the user hid it or HopCast's own screen is showing. */
  private fun applyMute() {
    val gl = stream?.getGlInterface() ?: return
    if (Live.hidden || Live.appVisible) gl.muteVideo() else gl.unMuteVideo()
  }

  private fun finish() {
    if (finishing) return
    finishing = true
    main.removeCallbacksAndMessages(null)
    bubble?.hide()
    bubble = null
    val s = stream
    stream = null
    try { if (s?.isStreaming == true) s.stopStream() } catch (e: Exception) { }
    try { if (s?.isRecording == true) s.stopRecord() } catch (e: Exception) { }
    try { s?.release() } catch (e: Exception) { }
    try { projection?.stop() } catch (e: Exception) { }
    projection = null
    val file = recordFile
    recordFile = null
    val last = Live.status
    Live.reset()
    Live.status = last
    Live.changed()
    if (file != null && file.exists() && file.length() > 0) {
      notify(notification("Saving your recording..."))
      publish(file)
    } else {
      done()
    }
  }

  private fun done() {
    finishing = false
    stopForeground(STOP_FOREGROUND_REMOVE)
    if (instance === this) instance = null
    stopSelf()
  }

  /** Copies the finished MP4 into the phone's Movies/HopCast folder, where the gallery and file manager see it. */
  private fun publish(file: File) {
    Thread {
      var saved = false
      try {
        val values = ContentValues().apply {
          put(MediaStore.Video.Media.DISPLAY_NAME, file.name)
          put(MediaStore.Video.Media.MIME_TYPE, "video/mp4")
          put(MediaStore.Video.Media.RELATIVE_PATH, "Movies/HopCast")
          put(MediaStore.Video.Media.IS_PENDING, 1)
        }
        val uri = contentResolver.insert(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, values)
        if (uri != null) {
          contentResolver.openOutputStream(uri)?.use { out -> file.inputStream().use { it.copyTo(out) } }
          values.clear()
          values.put(MediaStore.Video.Media.IS_PENDING, 0)
          contentResolver.update(uri, values, null, null)
          file.delete()
          saved = true
        }
      } catch (e: Exception) {
        saved = false
      }
      main.post {
        Live.status = if (saved) "Saved to Movies/HopCast" else "Couldn't copy the recording; it is in HopCast's own folder on the phone"
        Live.changed()
        toast(Live.status)
        done()
      }
    }.start()
  }

  private fun newRecordFile(): File {
    val dir = getExternalFilesDir(Environment.DIRECTORY_MOVIES) ?: filesDir
    dir.mkdirs()
    return File(dir, "HopCast-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date()) + ".mp4")
  }

  private fun endpoint(cfg: Config): String {
    val url = cfg.url.trim()
    val scheme = url.substringBefore("://", "").lowercase(Locale.US)
    return if ((scheme == "rtmp" || scheme == "rtmps") && cfg.key.isNotBlank()) url.trimEnd('/') + "/" + cfg.key.trim() else url
  }

  private fun displayIsPortrait(): Boolean {
    val display = getSystemService(DisplayManager::class.java).getDisplay(Display.DEFAULT_DISPLAY) ?: return true
    val size = Point()
    @Suppress("DEPRECATION")
    display.getRealSize(size)
    return size.y >= size.x
  }

  /** Never show the stream key in a message. */
  private fun clean(text: String): String {
    val key = config?.key?.trim().orEmpty()
    return if (key.length >= 4) text.replace(key, "***") else text
  }

  private fun toast(text: String) {
    main.post { Toast.makeText(applicationContext, text, Toast.LENGTH_LONG).show() }
  }

  private fun statusLine(): String = when {
    Live.waiting -> "Open your game: streaming starts once HopCast is out of view"
    Live.running && Live.streaming -> if (Live.hidden) "Live (screen hidden), recording to this phone" else "Live, recording to this phone"
    Live.running -> Live.status.ifEmpty { "Connecting..." } + if (recordFile != null) " Recording to this phone." else ""
    else -> Live.status
  }

  private fun refresh() {
    notify(notification(statusLine()))
    bubble?.update()
    Live.changed()
  }

  private fun notify(n: Notification) {
    getSystemService(NotificationManager::class.java).notify(NOTIF_ID, n)
  }

  private fun notification(text: String): Notification {
    val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
    val b = Notification.Builder(this, CHANNEL)
      .setSmallIcon(android.R.drawable.ic_menu_camera)
      .setContentTitle("HopCast")
      .setContentText(text)
      .setStyle(Notification.BigTextStyle().bigText(text))
      .setOngoing(true)
      .setContentIntent(open)
    if (Live.running) {
      b.addAction(action(if (Live.hidden) "Show screen" else "Hide screen", ACTION_HIDE, 2))
      b.addAction(action(if (Live.micOn) "Mic off" else "Mic on", ACTION_MIC, 3))
    }
    b.addAction(action("Stop", ACTION_STOP, 1))
    return b.build()
  }

  private fun action(title: String, act: String, code: Int): Notification.Action =
    Notification.Action.Builder(
      Icon.createWithResource(this, android.R.drawable.ic_menu_close_clear_cancel), title,
      PendingIntent.getService(this, code, Intent(this, CaptureService::class.java).setAction(act), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT),
    ).build()

  companion object {
    const val ACTION_START = "io.github.thestylewithin.hopcast.START"
    const val ACTION_STOP = "io.github.thestylewithin.hopcast.STOP"
    const val ACTION_HIDE = "io.github.thestylewithin.hopcast.HIDE"
    const val ACTION_MIC = "io.github.thestylewithin.hopcast.MIC"
    private const val CHANNEL = "capture"
    private const val NOTIF_ID = 1
    private const val SETTLE_MS = 1500L

    @Volatile var instance: CaptureService? = null

    fun send(context: android.content.Context, action: String) {
      context.startService(Intent(context, CaptureService::class.java).setAction(action))
    }
  }
}
