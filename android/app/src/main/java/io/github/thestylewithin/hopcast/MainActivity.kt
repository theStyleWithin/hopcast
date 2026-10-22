package io.github.thestylewithin.hopcast

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.media.projection.MediaProjectionManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.text.InputType
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast

/** Setup screen: where to stream, what to include, and the Go Live button. */
class MainActivity : Activity() {

  private lateinit var prefs: Prefs
  private lateinit var name: EditText
  private lateinit var url: EditText
  private lateinit var key: EditText
  private lateinit var gameSound: Switch
  private lateinit var mic: Switch
  private lateinit var bubble: Switch
  private lateinit var quality: Spinner
  private lateinit var shape: Spinner
  private lateinit var status: TextView
  private lateinit var goLive: Button

  private val refreshListener: () -> Unit = { refresh() }
  private var askedNotifications = false

  private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    prefs = Prefs(this)
    window.statusBarColor = Color.parseColor(BG)
    window.navigationBarColor = Color.parseColor(BG)

    val content = LinearLayout(this).apply {
      orientation = LinearLayout.VERTICAL
      setPadding(dp(18), dp(24), dp(18), dp(24))
    }
    content.addView(text("HopCast", 24f, Color.WHITE, bold = true))
    content.addView(text("Stream or record your screen while you use other apps.", 13f, DIM).apply { setPadding(0, dp(4), 0, dp(8)) })

    content.addView(section("GO LIVE TO"))
    name = field("Channel name", prefs.name, InputType.TYPE_CLASS_TEXT)
    url = field("Server address (rtmp://...)", prefs.url, InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI)
    key = field("Stream key", prefs.key, InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD)
    content.addView(name); content.addView(url); content.addView(key)
    content.addView(text("For an srt:// address, put the whole address above and leave the key empty.", 11f, DIM))

    content.addView(section("SOUND"))
    gameSound = toggle("Game sound", "Only from apps that allow it", prefs.gameSound)
    mic = toggle("Microphone", "Off until you turn it on", prefs.mic)
    content.addView(gameSound.parent as View)
    content.addView(mic.parent as View)

    content.addView(section("VIDEO"))
    quality = spinner(listOf("720p, 30 fps", "1080p, 30 fps", "720p, 60 fps"), prefs.quality)
    shape = spinner(listOf("Screen shape: auto, follows your game", "Screen shape: portrait", "Screen shape: landscape"), prefs.shape)
    content.addView(quality); content.addView(shape)
    content.addView(text("Always saved to this phone as an MP4 (Movies/HopCast) while you're live.", 12f, DIM).apply { setPadding(0, dp(8), 0, 0) })

    content.addView(section("ON SCREEN"))
    bubble = toggle("Floating bubble", "Quick buttons over your game: hide screen, microphone, stop", prefs.bubble)
    content.addView(bubble.parent as View)

    status = text("", 13f, Color.parseColor("#effc35")).apply { setPadding(0, dp(16), 0, dp(4)) }
    content.addView(status)

    goLive = Button(this).apply {
      isAllCaps = false
      textSize = 16f
      typeface = Typeface.DEFAULT_BOLD
      setTextColor(Color.parseColor("#111111"))
      background = GradientDrawable().apply { setColor(Color.parseColor("#effc35")); cornerRadius = dp(4).toFloat() }
      layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(54)).apply { topMargin = dp(8) }
      setOnClickListener { onGoLive() }
    }
    content.addView(goLive)

    setContentView(ScrollView(this).apply {
      setBackgroundColor(Color.parseColor(BG))
      isFillViewport = true
      addView(content)
    })
    refresh()
  }

  override fun onStart() {
    super.onStart()
    Live.addListener(refreshListener)
    Live.appVisible = true
    CaptureService.instance?.onAppVisibility(true)
    refresh()
  }

  override fun onStop() {
    savePrefs()
    Live.removeListener(refreshListener)
    Live.appVisible = false
    CaptureService.instance?.onAppVisibility(false)
    super.onStop()
  }

  private fun savePrefs() {
    prefs.name = name.text.toString().trim()
    prefs.url = url.text.toString().trim()
    prefs.key = key.text.toString().trim()
    prefs.gameSound = gameSound.isChecked
    prefs.mic = mic.isChecked
    prefs.bubble = bubble.isChecked
    prefs.quality = quality.selectedItemPosition
    prefs.shape = shape.selectedItemPosition
  }

  private fun refresh() {
    val active = Live.active()
    goLive.text = if (active) "Stop" else "Go Live"
    status.text = when {
      Live.waiting -> "Ready. Open your game: streaming starts once HopCast is out of view."
      Live.running && Live.streaming -> "Live, and recording to this phone."
      Live.running -> Live.status.ifEmpty { "Connecting..." }
      else -> Live.status
    }
    status.visibility = if (status.text.isEmpty()) View.GONE else View.VISIBLE
  }

  private fun onGoLive() {
    if (Live.active()) {
      CaptureService.send(this, CaptureService.ACTION_STOP)
      return
    }
    savePrefs()
    if (prefs.url.isBlank()) {
      Toast.makeText(this, "Enter the server address first", Toast.LENGTH_SHORT).show()
      return
    }
    if (!prefs.warned) {
      AlertDialog.Builder(this)
        .setTitle("Heads up, once")
        .setMessage(
          "Everything on your screen goes out live, including notifications and messages.\n\n" +
            "Tap the bubble any time to hide your screen. Your microphone stays off until you turn it on."
        )
        .setPositiveButton("Got it") { _, _ -> prefs.warned = true; onGoLive() }
        .show()
      return
    }
    if (Build.VERSION.SDK_INT >= 33 && !askedNotifications && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
      askedNotifications = true
      requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), REQ_NOTIFICATIONS)
      return
    }
    if (mic.isChecked && checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
      requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), REQ_MIC)
      return
    }
    if (bubble.isChecked && !Settings.canDrawOverlays(this)) {
      Toast.makeText(this, "Allow HopCast to show over other apps, then tap Go Live again", Toast.LENGTH_LONG).show()
      startActivity(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:$packageName")))
      return
    }
    @Suppress("DEPRECATION")
    startActivityForResult(getSystemService(MediaProjectionManager::class.java).createScreenCaptureIntent(), REQ_CAPTURE)
  }

  override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults)
    when (requestCode) {
      REQ_NOTIFICATIONS -> onGoLive() // notifications are nice to have; carry on either way
      REQ_MIC -> {
        if (grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED) {
          onGoLive()
        } else {
          mic.isChecked = false
          Toast.makeText(this, "Microphone stays off", Toast.LENGTH_SHORT).show()
          onGoLive()
        }
      }
    }
  }

  @Deprecated("Deprecated in Java")
  override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
    @Suppress("DEPRECATION")
    super.onActivityResult(requestCode, resultCode, data)
    if (requestCode != REQ_CAPTURE) return
    if (resultCode != RESULT_OK || data == null) {
      Toast.makeText(this, "Screen capture was not allowed", Toast.LENGTH_SHORT).show()
      return
    }
    val intent = Intent(this, CaptureService::class.java)
      .setAction(CaptureService.ACTION_START)
      .putExtra("resultCode", resultCode)
      .putExtra("data", data)
      .putExtra("url", prefs.url)
      .putExtra("key", prefs.key)
      .putExtra("sound", prefs.gameSound)
      .putExtra("mic", prefs.mic)
      .putExtra("quality", prefs.quality)
      .putExtra("shape", prefs.shape)
      .putExtra("bubble", prefs.bubble)
    startForegroundService(intent)
  }

  // --- small view helpers ---

  private fun text(value: String, size: Float, color: Int, bold: Boolean = false) = TextView(this).apply {
    text = value
    textSize = size
    setTextColor(color)
    if (bold) typeface = Typeface.DEFAULT_BOLD
  }

  private fun section(title: String) = text(title, 11f, DIM, bold = true).apply {
    letterSpacing = 0.1f
    setPadding(0, dp(20), 0, dp(6))
  }

  private fun field(hint: String, value: String, inputType: Int) = EditText(this).apply {
    this.hint = hint
    setText(value)
    this.inputType = inputType
    setSingleLine()
    setTextColor(Color.WHITE)
    setHintTextColor(Color.parseColor("#777777"))
    setBackgroundColor(Color.parseColor("#1e1e1f"))
    setPadding(dp(12), dp(12), dp(12), dp(12))
    importantForAutofill = View.IMPORTANT_FOR_AUTOFILL_NO
    layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(8) }
  }

  private fun toggle(title: String, subtitle: String, checked: Boolean): Switch {
    val sw = Switch(this).apply { isChecked = checked }
    val row = LinearLayout(this).apply {
      orientation = LinearLayout.HORIZONTAL
      gravity = Gravity.CENTER_VERTICAL
      setPadding(0, dp(8), 0, dp(8))
    }
    val labels = LinearLayout(this).apply {
      orientation = LinearLayout.VERTICAL
      layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
    }
    labels.addView(text(title, 15f, Color.WHITE))
    labels.addView(text(subtitle, 12f, DIM))
    row.addView(labels)
    row.addView(sw)
    return sw
  }

  private fun spinner(options: List<String>, selected: Int) = Spinner(this).apply {
    adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, options)
    setSelection(selected)
    layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(8) }
  }

  private companion object {
    const val BG = "#272727"
    val DIM = Color.parseColor("#8a8a8a")
    const val REQ_CAPTURE = 1
    const val REQ_NOTIFICATIONS = 2
    const val REQ_MIC = 3
  }
}
