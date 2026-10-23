package io.github.thestylewithin.hopcast

import android.Manifest
import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.media.projection.MediaProjectionManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.text.InputType
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.Spinner
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import java.io.File

/**
 * The studio: channels across the top, a preview of the layout, tabs for scenes, sources and the audio mixer,
 * and Go Live / Rec at the bottom. Once you go live HopCast steps aside and keeps capturing over other apps.
 */
class MainActivity : Activity() {

  private lateinit var prefs: Prefs
  private lateinit var channelRow: LinearLayout
  private lateinit var preview: PreviewView
  private lateinit var shapePill: TextView
  private lateinit var infoText: TextView
  private lateinit var editButton: Button
  private lateinit var removeButton: Button
  private lateinit var tabViews: List<TextView>
  private lateinit var panel: FrameLayout
  private lateinit var timer: TextView
  private lateinit var goLive: Button
  private lateinit var rec: Button

  private val ui = Handler(Looper.getMainLooper())
  private var tab = 1
  private var askedNotifications = false
  private var pendingStream = true
  private var pendingPick: ((File) -> Unit)? = null
  private var pendingPickFolder = "sources"
  private var pendingCamera: (() -> Unit)? = null
  private var askedCamera = false

  private val refreshListener: () -> Unit = { refreshAll() }
  private val tick = object : Runnable {
    override fun run() {
      refreshBottom()
      ui.postDelayed(this, 1000)
    }
  }

  private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    Studio.load(applicationContext)
    prefs = Prefs(this)
    window.statusBarColor = Color.parseColor(BG)
    window.navigationBarColor = Color.parseColor(BG)
    setContentView(buildUi())
    refreshAll()
  }

  override fun onStart() {
    super.onStart()
    Live.addListener(refreshListener)
    Live.appVisible = true
    CaptureService.instance?.onAppVisibility(true)
    ui.post(tick)
    refreshAll()
  }

  override fun onStop() {
    Studio.save()
    Live.removeListener(refreshListener)
    ui.removeCallbacks(tick)
    Live.appVisible = false
    CaptureService.instance?.onAppVisibility(false)
    super.onStop()
  }

  // ---------------------------------------------------------------- layout

  private fun buildUi(): View {
    val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setBackgroundColor(Color.parseColor(BG)) }

    // top bar
    val top = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(dp(14), dp(10), dp(14), dp(6)) }
    top.addView(label("☰", 20f, Color.WHITE).apply { setPadding(0, 0, dp(12), 0); setOnClickListener { startActivity(Intent(this@MainActivity, BlocklistActivity::class.java)) } })
    top.addView(label("HopCast", 17f, Color.WHITE, bold = true))
    top.addView(label("  v" + versionName(), 11f, DIM))
    top.addView(View(this), LinearLayout.LayoutParams(0, 1, 1f))
    top.addView(label("⚙", 22f, Color.WHITE).apply { setOnClickListener { settingsDialog() } })
    root.addView(top)

    // channels
    val channelBar = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(dp(12), 0, 0, 0) }
    channelBar.addView(label("My\nChannel", 12f, Color.WHITE, bold = true).apply { setPadding(0, 0, dp(10), 0) })
    channelRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
    channelBar.addView(HorizontalScrollView(this).apply { isHorizontalScrollBarEnabled = false; addView(channelRow) })
    root.addView(channelBar, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(56)))
    root.addView(divider())

    // preview
    preview = PreviewView(this).apply {
      onSelect = { refreshInfo(); refreshPanel() }
      onChanged = { refreshInfo() }
    }
    shapePill = label("", 10f, Color.WHITE).apply {
      background = GradientDrawable().apply { setColor(Color.parseColor("#aa000000")); cornerRadius = dp(10).toFloat() }
      setPadding(dp(8), dp(3), dp(8), dp(3))
      setOnClickListener { prefs.shape = (prefs.shape + 1) % 3; refreshShape() }
    }
    val previewBox = FrameLayout(this)
    previewBox.addView(preview, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
    previewBox.addView(shapePill, FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP or Gravity.END).apply { setMargins(0, dp(14), dp(16), 0) })
    root.addView(previewBox)

    // selected source
    val info = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(dp(12), dp(6), dp(12), dp(6)) }
    infoText = label("", 12f, Color.parseColor("#c8c8c8"))
    info.addView(infoText, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
    editButton = smallButton("Edit") { editSelected() }
    removeButton = smallButton("Remove") { removeSelected() }
    info.addView(editButton); info.addView(removeButton)
    root.addView(info)

    // tabs
    val tabs = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
    tabViews = listOf("Scenes", "Sources", "Audio Mixer").mapIndexed { i, name ->
      label(name, 13f, DIM, bold = true).apply {
        gravity = Gravity.CENTER
        setPadding(0, dp(11), 0, dp(11))
        setOnClickListener { tab = i; refreshTabs(); refreshPanel() }
      }
    }
    tabViews.forEach { tabs.addView(it, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)) }
    root.addView(tabs)
    root.addView(divider())

    // panel
    panel = FrameLayout(this).apply { setPadding(dp(14), dp(8), dp(14), dp(8)) }
    root.addView(ScrollView(this).apply { addView(panel) }, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))

    // bottom bar
    root.addView(divider())
    val bottom = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(dp(12), dp(8), dp(12), dp(8)) }
    timer = label("", 11f, Color.parseColor("#cfcfcf"), bold = true)
    bottom.addView(timer, LinearLayout.LayoutParams(dp(78), ViewGroup.LayoutParams.WRAP_CONTENT))
    goLive = bigButton("Go Live", "#effc35", "#111111") { if (Live.active()) stopCapture() else startCapture(true) }
    rec = bigButton("Rec", "#c34151", "#ffffff") { startCapture(false) }
    bottom.addView(goLive, LinearLayout.LayoutParams(0, dp(44), 1f).apply { rightMargin = dp(10) })
    bottom.addView(rec, LinearLayout.LayoutParams(dp(84), dp(44)))
    root.addView(bottom)
    return root
  }

  private fun refreshAll() {
    refreshHint(); refreshChannels(); refreshShape(); refreshInfo(); refreshTabs(); refreshPanel(); refreshBottom()
  }

  private fun refreshShape() {
    preview.portrait = prefs.shape == 1
    shapePill.text = when (prefs.shape) { 1 -> "Portrait"; 2 -> "Landscape"; else -> "Auto" }
  }

  private fun refreshInfo() {
    val s = preview.selected
    infoText.text = when {
      s == null -> "No source selected. Tap one in the preview or the list."
      s.kind == "game" -> "Game: the stream shows only ${s.name}"
      else -> "${s.name}: drag to move, pinch to resize"
    }
    editButton.isEnabled = s != null
    removeButton.isEnabled = s != null
    editButton.alpha = if (s != null) 1f else 0.4f
    removeButton.alpha = if (s != null) 1f else 0.4f
  }

  private fun refreshTabs() {
    tabViews.forEachIndexed { i, v ->
      v.setTextColor(if (i == tab) Color.WHITE else DIM)
      v.background = if (i == tab) GradientDrawable().apply { setColor(Color.parseColor("#2f2f2f")) } else null
    }
  }

  private fun refreshBottom() {
    val seconds = if (Live.startedMs > 0) ((System.currentTimeMillis() - Live.startedMs) / 1000).toInt() else 0
    timer.text = (if (Live.running) (if (Live.streaming) "LIVE" else "REC") else "LIVE") + "\n" +
      String.format("%02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60)
    goLive.text = if (Live.active()) "Stop" else "Go Live"
    rec.visibility = if (Live.active()) View.GONE else View.VISIBLE
  }

  // ---------------------------------------------------------------- channels

  private fun refreshChannels() {
    channelRow.removeAllViews()
    val first = Studio.channels.firstOrNull { it.enabled }
    Studio.channels.forEach { c ->
      val chip = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        setPadding(0, 0, dp(14), 0)
        alpha = if (c.enabled) 1f else 0.45f
        setOnClickListener {
          if (Live.active()) { Toast.makeText(this@MainActivity, "Stop first to change channels", Toast.LENGTH_SHORT).show(); return@setOnClickListener }
          c.enabled = !c.enabled; Studio.save(); refreshChannels()
        }
        setOnLongClickListener { channelDialog(c); true }
      }
      chip.addView(ImageView(this).apply { setImageBitmap(avatar(c.name, c.icon, 30)); layoutParams = LinearLayout.LayoutParams(dp(30), dp(30)).apply { rightMargin = dp(6) } })
      chip.addView(label(c.name, 12f, Color.parseColor("#d8d8d8")))
      val badge = when {
        !c.enabled -> "Off"
        Live.streaming && c === first -> "Online"
        else -> ""
      }
      if (badge.isNotEmpty()) {
        chip.addView(label(badge, 9f, if (badge == "Online") Color.WHITE else Color.parseColor("#999999"), bold = true).apply {
          background = GradientDrawable().apply { setColor(Color.parseColor(if (badge == "Online") "#c34151" else "#3a3a3a")); cornerRadius = dp(3).toFloat() }
          setPadding(dp(4), dp(1), dp(4), dp(1))
          (layoutParams as? LinearLayout.LayoutParams)?.leftMargin = dp(6)
        })
      }
      channelRow.addView(chip)
    }
    channelRow.addView(label("+  Add", 13f, Color.WHITE, bold = true).apply { setPadding(0, 0, dp(14), 0); setOnClickListener { channelDialog(null) } })
  }

  private fun channelDialog(existing: Studio.Channel?) {
    val form = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(12), dp(20), 0) }
    val name = field("Channel name", existing?.name ?: "", InputType.TYPE_CLASS_TEXT)
    val url = field("Server address (rtmp://...)", existing?.url ?: "", InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI)
    val key = field("Stream key", if (existing != null) prefs.channelKey(existing.id) else "", InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD)
    form.addView(name); form.addView(url); form.addView(key)
    form.addView(label("For an srt:// address, put the whole address above and leave the key empty.", 11f, DIM).apply { setPadding(0, dp(6), 0, dp(8)) })
    var icon = existing?.icon ?: ""
    val avatarView = ImageView(this).apply { layoutParams = LinearLayout.LayoutParams(dp(44), dp(44)) }
    fun showAvatar() = avatarView.setImageBitmap(avatar(name.text.toString().ifBlank { "R" }, icon, 44))
    showAvatar()
    val pictureRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
    pictureRow.addView(avatarView)
    pictureRow.addView(smallButton("Choose a picture...") { pickPicture("channel-icons") { f -> icon = f.path; showAvatar() } })
    pictureRow.addView(smallButton("Use the default") { icon = ""; showAvatar() })
    form.addView(pictureRow)
    val builder = AlertDialog.Builder(this)
      .setTitle(if (existing == null) "Add a channel" else "Edit channel")
      .setView(ScrollView(this).apply { addView(form) })
      .setNegativeButton("Cancel", null)
      .setPositiveButton("Save") { _, _ ->
        val n = name.text.toString().trim().ifBlank { "My channel" }
        val u = url.text.toString().trim()
        if (u.isBlank()) { Toast.makeText(this, "Enter the server address", Toast.LENGTH_SHORT).show(); return@setPositiveButton }
        val c = existing ?: Studio.Channel(Studio.newId(), n, u, true, icon).also { Studio.channels.add(it) }
        c.name = n; c.url = u; c.icon = icon
        prefs.setChannelKey(c.id, key.text.toString().trim())
        Studio.save(); refreshChannels()
      }
    if (existing != null) {
      builder.setNeutralButton("Delete") { _, _ ->
        prefs.removeChannelKey(existing.id)
        Studio.channels.remove(existing); Studio.save(); refreshChannels()
      }
    }
    builder.show()
  }

  /** A round picture for a channel: the chosen image, or its first letter on a light circle. */
  private fun avatar(name: String, iconPath: String, sizeDp: Int): Bitmap {
    val size = dp(sizeDp)
    val out = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
    val canvas = Canvas(out)
    val icon = if (iconPath.isNotEmpty()) Studio.decode(iconPath, size) else null
    if (icon != null) {
      val clip = Path().apply { addCircle(size / 2f, size / 2f, size / 2f, Path.Direction.CW) }
      canvas.clipPath(clip)
      val scale = Math.max(size / icon.width.toFloat(), size / icon.height.toFloat())
      val w = icon.width * scale
      val h = icon.height * scale
      canvas.drawBitmap(icon, null, android.graphics.RectF((size - w) / 2f, (size - h) / 2f, (size + w) / 2f, (size + h) / 2f), Paint(Paint.FILTER_BITMAP_FLAG))
    } else {
      canvas.drawCircle(size / 2f, size / 2f, size / 2f, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#e9e9e9") })
      val text = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.parseColor("#222222"); textSize = size * 0.5f; typeface = Typeface.DEFAULT_BOLD; textAlign = Paint.Align.CENTER }
      canvas.drawText(name.take(1).uppercase().ifEmpty { "R" }, size / 2f, size / 2f - (text.descent() + text.ascent()) / 2f, text)
    }
    return out
  }

  // ---------------------------------------------------------------- tabs

  private fun refreshPanel() {
    panel.removeAllViews()
    panel.addView(when (tab) { 0 -> scenesPanel(); 1 -> sourcesPanel(); else -> mixerPanel() })
  }

  private fun header(title: String, onAdd: (() -> Unit)?): View {
    val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, 0, 0, dp(6)) }
    row.addView(label(title, 13f, Color.WHITE, bold = true), LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
    if (onAdd != null) row.addView(label("+", 26f, Color.WHITE).apply { setPadding(dp(14), 0, dp(4), 0); setOnClickListener { onAdd() } })
    return row
  }

  private fun sourcesPanel(): View {
    val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
    list.addView(header("Sources") { addSourceMenu() })
    val bound = Studio.scene().sources.any { it.kind == "game" && it.visible }
    list.addView(sourceRow(null, if (bound) "Whole screen (not used while a game is bound)" else "Whole screen: home screen and every app", if (bound) "Off" else "Always on"))
    Studio.scene().sources.reversed().forEach { s ->
      val title = when (s.kind) {
        "game" -> "Game: ${s.name} (only this app)"
        "text" -> s.name + " (text)"
        "gif" -> s.name + " (animated)"
        "camera" -> s.name + " (camera)"
        else -> s.name + " (image)"
      }
      list.addView(sourceRow(s, title, if (s.visible) (if (s.kind == "game") "Bound" else "Shown") else "Hidden"))
    }
    return list
  }

  private fun sourceRow(s: Studio.Source?, title: String, state: String): View {
    val row = LinearLayout(this).apply {
      orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(8), 0, dp(8))
      if (s != null && s === preview.selected) setBackgroundColor(Color.parseColor("#33effc35"))
      setOnClickListener { if (s != null) { preview.selected = s; refreshInfo(); refreshPanel() } }
    }
    val thumb = ImageView(this).apply {
      scaleType = ImageView.ScaleType.CENTER_CROP
      layoutParams = LinearLayout.LayoutParams(dp(40), dp(26)).apply { rightMargin = dp(10) }
      if (s == null) background = GradientDrawable(GradientDrawable.Orientation.TL_BR, intArrayOf(Color.parseColor("#12243b"), Color.parseColor("#3d2535")))
      else if (s.kind == "game") { try { setImageDrawable(packageManager.getApplicationIcon(s.text)) } catch (e: Exception) { } }
      else Studio.decode(s.file, 96)?.let { setImageBitmap(it) }
    }
    row.addView(thumb)
    row.addView(label(title, 13f, Color.WHITE), LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
    row.addView(label(state, 12f, DIM).apply {
      setPadding(dp(10), dp(4), dp(4), dp(4))
      if (s != null) setOnClickListener { s.visible = !s.visible; Studio.save(); preview.reload(); refreshHint(); refreshPanel() }
    })
    return row
  }

  private fun scenesPanel(): View {
    val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
    list.addView(header("Scenes") {
      textDialog("New scene", "Scene name", "Scene ${Studio.scenes.size + 1}") { n ->
        Studio.scenes.add(Studio.Scene(Studio.newId(), n)); Studio.activeScene = Studio.scenes.size - 1
        preview.selected = null; preview.reload(); Studio.save(); refreshAll()
      }
    })
    Studio.scenes.forEachIndexed { i, sc ->
      list.addView(label(sc.name, 15f, if (i == Studio.activeScene) Color.parseColor("#effc35") else Color.parseColor("#dddddd"), bold = i == Studio.activeScene).apply {
        setPadding(0, dp(10), 0, dp(10))
        setOnClickListener {
          Studio.activeScene = i; preview.selected = null; preview.reload(); Studio.save(); refreshAll()
        }
        setOnLongClickListener {
          AlertDialog.Builder(this@MainActivity).setTitle(sc.name).setItems(arrayOf("Rename", "Delete")) { _, which ->
            if (which == 0) textDialog("Rename scene", "Scene name", sc.name) { n -> sc.name = n; Studio.save(); refreshPanel() }
            else if (Studio.scenes.size > 1) {
              Studio.scenes.remove(sc); Studio.activeScene = 0; preview.selected = null; preview.reload(); Studio.save(); refreshAll()
            } else Toast.makeText(this@MainActivity, "Keep at least one scene", Toast.LENGTH_SHORT).show()
          }.show()
          true
        }
      })
    }
    list.addView(label("The scene that is selected when you go live is the one that goes out.", 11f, DIM).apply { setPadding(0, dp(10), 0, 0) })
    return list
  }

  private fun mixerPanel(): View {
    val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
    list.addView(header("Audio Mixer", null))
    list.addView(toggleRow("Game sound", "Only from apps that allow it", prefs.gameSound) { prefs.gameSound = it })
    list.addView(toggleRow("Microphone", "Off until you turn it on", if (Live.running) Live.micOn else prefs.mic) { on ->
      if (on && checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
        requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), REQ_MIC_TOGGLE)
      } else if (Live.running) {
        if (Live.micOn != on) CaptureService.send(this, CaptureService.ACTION_MIC)
      } else prefs.mic = on
    })
    return list
  }

  private fun toggleRow(title: String, subtitle: String, checked: Boolean, onChange: (Boolean) -> Unit): View {
    val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(10), 0, dp(10)) }
    val labels = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
    labels.addView(label(title, 15f, Color.WHITE))
    labels.addView(label(subtitle, 12f, DIM))
    row.addView(labels, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
    row.addView(Switch(this).apply { isChecked = checked; setOnCheckedChangeListener { _, on -> onChange(on) } })
    return row
  }

  // ---------------------------------------------------------------- sources

  private fun addSourceMenu() {
    AlertDialog.Builder(this).setTitle("Add a source")
      .setItems(arrayOf("Image (PNG, JPG, WebP)", "Animated picture (GIF)", "Text", "Camera", "Game (show only one app)")) { _, which ->
        when (which) {
          0, 1 -> pickPicture("sources") { f -> addPictureSource(f) }
          2 -> textSourceDialog(null)
          3 -> cameraMenu()
          else -> gameMenu()
        }
      }.show()
  }

  private fun appLabel(pkg: String): String = try {
    packageManager.getApplicationLabel(packageManager.getApplicationInfo(pkg, 0)).toString()
  } catch (e: Exception) {
    pkg
  }

  /** Binds the stream to one app: only that app is ever shown; anything else stays black until you are back in it. */
  private fun gameMenu() {
    val last = AppWatcher.lastAppBeforeUs(this)
    val items = mutableListOf<String>()
    val actions = mutableListOf<() -> Unit>()
    if (last != null) {
      val label = appLabel(last)
      items.add("The app you were just in: $label")
      actions.add { setGame(last, label) }
    }
    items.add("Choose from my apps...")
    actions.add {
      @Suppress("DEPRECATION")
      startActivityForResult(Intent(this, BlocklistActivity::class.java).putExtra("pick", true), REQ_GAME)
    }
    if (Studio.scene().sources.any { it.kind == "game" }) {
      items.add("Show the whole screen instead")
      actions.add {
        Studio.scene().sources.removeAll { it.kind == "game" }
        preview.selected = null; Studio.save(); refreshAll()
      }
    }
    AlertDialog.Builder(this).setTitle("Game").setItems(items.toTypedArray()) { _, i -> actions[i]() }.show()
  }

  private fun setGame(pkg: String, label: String) {
    Studio.scene().sources.removeAll { it.kind == "game" }
    val game = Studio.Source(Studio.newId(), "game", label, true, "", pkg, Color.WHITE, 0f, 0f, 0f, 1f)
    Studio.scene().sources.add(0, game)
    preview.selected = null
    Studio.save()
    refreshAll()
    Toast.makeText(this, "Bound to $label. Only it will show on the stream.", Toast.LENGTH_LONG).show()
  }

  private fun refreshHint() {
    val g = Studio.scene().sources.firstOrNull { it.kind == "game" && it.visible }
    preview.hint = if (g != null) "GAME: " + g.name.uppercase() else "WHOLE SCREEN"
  }

  private fun withCamera(then: () -> Unit) {
    if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
      then()
    } else {
      pendingCamera = then
      requestPermissions(arrayOf(Manifest.permission.CAMERA), REQ_CAMERA)
    }
  }

  private fun cameraMenu() {
    val front = CameraFeed.hasFront(this)
    val back = CameraFeed.hasBack(this)
    val options = mutableListOf<Pair<String, String>>()
    if (front) options.add("Front camera" to "front")
    if (back) options.add("Back camera" to "back")
    if (front && back) {
      options.add("Switch between front and back" to "switch")
      options.add("Both at the same time" to "both")
    }
    if (options.isEmpty()) { Toast.makeText(this, "This phone has no camera HopCast can use", Toast.LENGTH_LONG).show(); return }
    AlertDialog.Builder(this).setTitle("Camera").setItems(options.map { it.first }.toTypedArray()) { _, i ->
      val mode = options[i].second
      withCamera { addCamera(mode) }
    }.show()
  }

  private fun addCamera(mode: String) {
    when (mode) {
      "front" -> addCameraSource("front", "Front camera")
      "back" -> addCameraSource("back", "Back camera")
      "switch" -> addCameraSource("switch", "Camera (front or back)")
      else -> {
        if (!CameraFeed.bothAtOnce(this)) {
          Toast.makeText(this, "This phone can't run both cameras at the same time. Use \"Switch between front and back\" instead.", Toast.LENGTH_LONG).show()
          return
        }
        addCameraSource("front", "Front camera")
        addCameraSource("back", "Back camera")
      }
    }
  }

  private fun addCameraSource(mode: String, label: String) {
    val card = Studio.renderCameraLabel(this, label.take(22))
    if (card == null) { Toast.makeText(this, "Couldn't add the camera", Toast.LENGTH_SHORT).show(); return }
    addSource(Studio.Source(Studio.newId(), "camera", label, true, card.path, mode, Color.WHITE, 5f, 5f, 28f, 480f / 360f))
  }

  private fun addPictureSource(file: File) {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeFile(file.path, bounds)
    if (bounds.outWidth <= 0 || bounds.outHeight <= 0) {
      file.delete()
      Toast.makeText(this, "That file isn't a picture HopCast can read", Toast.LENGTH_LONG).show()
      return
    }
    val gif = file.extension.lowercase() == "gif"
    addSource(Studio.Source(Studio.newId(), if (gif) "gif" else "image", if (gif) "Animated picture" else "Image", true, file.path, "", Color.WHITE, 5f, 5f, 30f, bounds.outWidth.toFloat() / bounds.outHeight))
  }

  private fun addSource(s: Studio.Source) {
    val offset = Studio.scene().sources.size * 3f
    s.x += offset; s.y += offset
    Studio.scene().sources.add(s)
    Studio.save()
    preview.reload()
    preview.selected = s
    refreshInfo(); refreshPanel()
  }

  private fun textSourceDialog(existing: Studio.Source?) {
    val colors = linkedMapOf("White" to Color.WHITE, "Yellow" to Color.parseColor("#effc35"), "Red" to Color.parseColor("#ff5a5f"), "Green" to Color.parseColor("#5bd75b"), "Blue" to Color.parseColor("#5aa8ff"), "Black" to Color.BLACK)
    val form = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(12), dp(20), 0) }
    val text = field("Your text", existing?.text ?: "", InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE).apply { setSingleLine(false); minLines = 2 }
    val color = Spinner(this).apply {
      adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, colors.keys.toList())
      setSelection(colors.values.indexOf(existing?.color ?: Color.WHITE).coerceAtLeast(0))
    }
    form.addView(text); form.addView(color)
    AlertDialog.Builder(this).setTitle(if (existing == null) "Add text" else "Edit text").setView(form)
      .setNegativeButton("Cancel", null)
      .setPositiveButton("Save") { _, _ ->
        val value = text.text.toString().trim()
        if (value.isEmpty()) return@setPositiveButton
        val c = colors.values.toList()[color.selectedItemPosition]
        val rendered = Studio.renderText(this, value, c)
        if (rendered == null) { Toast.makeText(this, "Couldn't draw that text", Toast.LENGTH_SHORT).show(); return@setPositiveButton }
        if (existing == null) {
          addSource(Studio.Source(Studio.newId(), "text", value.lines().first().take(20), true, rendered.first.path, value, c, 5f, 5f, 30f, rendered.second))
        } else {
          File(existing.file).delete()
          existing.file = rendered.first.path; existing.text = value; existing.color = c; existing.aspect = rendered.second; existing.name = value.lines().first().take(20)
          Studio.save(); preview.reload(); refreshPanel(); refreshInfo()
        }
      }.show()
  }

  private fun editSelected() {
    val s = preview.selected ?: return
    if (s.kind == "game") gameMenu()
    else if (s.kind == "text") textSourceDialog(s)
    else textDialog("Rename", "Name", s.name) { n -> s.name = n; Studio.save(); refreshInfo(); refreshPanel() }
  }

  private fun removeSelected() {
    val s = preview.selected ?: return
    AlertDialog.Builder(this).setTitle("Remove ${s.name}?").setNegativeButton("Cancel", null)
      .setPositiveButton("Remove") { _, _ ->
        Studio.scene().sources.remove(s)
        if (File(s.file).absolutePath.contains("/sources/")) File(s.file).delete()
        preview.selected = null; Studio.save(); preview.reload(); refreshHint(); refreshInfo(); refreshPanel()
      }.show()
  }

  private fun pickPicture(folder: String, then: (File) -> Unit) {
    pendingPick = then
    pendingPickFolder = folder
    @Suppress("DEPRECATION")
    startActivityForResult(Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("image/*"), REQ_PICK)
  }

  private fun textDialog(title: String, hint: String, initial: String, done: (String) -> Unit) {
    val input = field(hint, initial, InputType.TYPE_CLASS_TEXT)
    val form = FrameLayout(this).apply { setPadding(dp(20), dp(8), dp(20), 0); addView(input) }
    AlertDialog.Builder(this).setTitle(title).setView(form).setNegativeButton("Cancel", null)
      .setPositiveButton("OK") { _, _ -> input.text.toString().trim().takeIf { it.isNotEmpty() }?.let(done) }.show()
  }

  // ---------------------------------------------------------------- settings

  private fun settingsDialog() {
    val form = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(12), dp(20), 0) }
    form.addView(label("Quality", 12f, DIM))
    val quality = Spinner(this).apply {
      adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, listOf("720p, 30 fps", "1080p, 30 fps", "720p, 60 fps"))
      setSelection(prefs.quality)
    }
    form.addView(quality)
    val bubble = Switch(this).apply { text = "Floating bubble over other apps"; isChecked = prefs.bubble; setTextColor(Color.WHITE) }
    form.addView(bubble, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(14) })
    val n = Studio.blocked.size
    form.addView(Button(this).apply {
      isAllCaps = false
      text = "App block list ($n)"
      setOnClickListener { startActivity(Intent(this@MainActivity, BlocklistActivity::class.java)) }
    }, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(10) })
    AlertDialog.Builder(this).setTitle("Settings").setView(form).setNegativeButton("Cancel", null)
      .setPositiveButton("Save") { _, _ -> prefs.quality = quality.selectedItemPosition; prefs.bubble = bubble.isChecked }.show()
  }

  // ---------------------------------------------------------------- going live

  private fun stopCapture() = CaptureService.send(this, CaptureService.ACTION_STOP)

  private fun startCapture(stream: Boolean) {
    pendingStream = stream
    Studio.save()
    val channel = Studio.channels.firstOrNull { it.enabled && it.url.isNotBlank() }
    if (stream && channel == null) {
      Toast.makeText(this, "Add a channel first", Toast.LENGTH_SHORT).show()
      channelDialog(null)
      return
    }
    if (!prefs.warned) {
      AlertDialog.Builder(this).setTitle("Heads up, once")
        .setMessage("Everything on your screen goes out live and into the recording, including notifications and messages.\n\nApps on your block list go black. Tap the bubble any time to hide your screen. Your microphone stays off until you turn it on.")
        .setPositiveButton("Got it") { _, _ -> prefs.warned = true; startCapture(stream) }.show()
      return
    }
    if ((Studio.blocked.isNotEmpty() || Studio.scene().sources.any { it.kind == "game" && it.visible }) && !AppWatcher.allowed(this)) {
      AlertDialog.Builder(this).setTitle("Allow usage access")
        .setMessage("HopCast needs to see which app is open to keep other apps off the stream (your block list or your game). Android needs you to allow \"usage access\" once. It only shows HopCast which app is open, nothing inside it. Then tap again.")
        .setNegativeButton("Cancel", null)
        .setPositiveButton("Allow") { _, _ ->
          try { startActivity(Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS, Uri.parse("package:$packageName"))) }
          catch (e: Exception) { startActivity(Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS)) }
        }.show()
      return
    }
    if (Studio.scene().sources.any { it.kind == "camera" && it.visible } && !askedCamera &&
      checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
      askedCamera = true
      withCamera { startCapture(stream) }
      return
    }
    if (Build.VERSION.SDK_INT >= 33 && !askedNotifications && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
      askedNotifications = true
      requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), REQ_NOTIFICATIONS)
      return
    }
    if (prefs.mic && checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
      requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), REQ_MIC)
      return
    }
    if (prefs.bubble && !Settings.canDrawOverlays(this)) {
      Toast.makeText(this, "Allow HopCast to show over other apps, then tap again", Toast.LENGTH_LONG).show()
      startActivity(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:$packageName")))
      return
    }
    if (stream && Studio.channels.count { it.enabled } > 1) {
      Toast.makeText(this, "Streaming to ${channel!!.name} for now; several channels at once is coming next", Toast.LENGTH_LONG).show()
    }
    @Suppress("DEPRECATION")
    startActivityForResult(getSystemService(MediaProjectionManager::class.java).createScreenCaptureIntent(), REQ_CAPTURE)
  }

  override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
    super.onRequestPermissionsResult(requestCode, permissions, grantResults)
    val granted = grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED
    when (requestCode) {
      REQ_NOTIFICATIONS -> startCapture(pendingStream) // nice to have; carry on either way
      REQ_MIC -> {
        if (!granted) { prefs.mic = false; Toast.makeText(this, "Microphone stays off", Toast.LENGTH_SHORT).show() }
        startCapture(pendingStream)
      }
      REQ_CAMERA -> {
        val then = pendingCamera
        pendingCamera = null
        if (granted) then?.invoke() else {
          Toast.makeText(this, "The camera stays off", Toast.LENGTH_SHORT).show()
          if (askedCamera) startCapture(pendingStream)
        }
      }
      REQ_MIC_TOGGLE -> {
        if (granted) { if (Live.running) CaptureService.send(this, CaptureService.ACTION_MIC) else prefs.mic = true }
        else Toast.makeText(this, "Microphone stays off", Toast.LENGTH_SHORT).show()
        refreshPanel()
      }
    }
  }

  @Deprecated("Deprecated in Java")
  override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
    @Suppress("DEPRECATION")
    super.onActivityResult(requestCode, resultCode, data)
    if (requestCode == REQ_GAME) {
      val pkg = data?.getStringExtra("pkg")
      if (resultCode == RESULT_OK && pkg != null) setGame(pkg, data?.getStringExtra("label") ?: appLabel(pkg))
      return
    }
    if (requestCode == REQ_PICK) {
      val uri = data?.data
      val callback = pendingPick
      pendingPick = null
      if (resultCode != RESULT_OK || uri == null || callback == null) return
      val type = contentResolver.getType(uri).orEmpty()
      val ext = when { type.contains("gif") -> "gif"; type.contains("png") -> "png"; type.contains("webp") -> "webp"; else -> "jpg" }
      val file = Studio.importPicture(this, uri, pendingPickFolder, ext)
      if (file == null) Toast.makeText(this, "Couldn't read that picture", Toast.LENGTH_SHORT).show() else callback(file)
      return
    }
    if (requestCode != REQ_CAPTURE) return
    if (resultCode != RESULT_OK || data == null) {
      Toast.makeText(this, "Screen capture was not allowed", Toast.LENGTH_SHORT).show()
      return
    }
    val channel = Studio.channels.firstOrNull { it.enabled && it.url.isNotBlank() }
    val intent = Intent(this, CaptureService::class.java)
      .setAction(CaptureService.ACTION_START)
      .putExtra("resultCode", resultCode)
      .putExtra("data", data)
      .putExtra("stream", pendingStream)
      .putExtra("url", channel?.url.orEmpty())
      .putExtra("key", channel?.let { prefs.channelKey(it.id) }.orEmpty())
      .putExtra("sound", prefs.gameSound)
      .putExtra("mic", prefs.mic)
      .putExtra("quality", prefs.quality)
      .putExtra("shape", prefs.shape)
      .putExtra("bubble", prefs.bubble)
    startForegroundService(intent)
    moveTaskToBack(true) // step aside so your game is what's on screen
  }

  // ---------------------------------------------------------------- small view helpers

  private fun label(value: String, size: Float, color: Int, bold: Boolean = false) = TextView(this).apply {
    text = value; textSize = size; setTextColor(color)
    if (bold) typeface = Typeface.DEFAULT_BOLD
  }

  private fun divider() = View(this).apply { setBackgroundColor(Color.parseColor("#383838")); layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 1) }

  private fun smallButton(text: String, onClick: () -> Unit) = Button(this).apply {
    this.text = text; isAllCaps = false; textSize = 11f; setTextColor(Color.parseColor("#dddddd"))
    background = GradientDrawable().apply { setColor(Color.parseColor("#3a3a3a")); cornerRadius = dp(2).toFloat() }
    minHeight = 0; minimumHeight = 0; setPadding(dp(10), dp(6), dp(10), dp(6))
    layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(32)).apply { leftMargin = dp(8) }
    setOnClickListener { onClick() }
  }

  private fun bigButton(text: String, bg: String, fg: String, onClick: () -> Unit) = Button(this).apply {
    this.text = text; isAllCaps = false; textSize = 15f; typeface = Typeface.DEFAULT_BOLD; setTextColor(Color.parseColor(fg))
    background = GradientDrawable().apply { setColor(Color.parseColor(bg)); cornerRadius = dp(3).toFloat() }
    setOnClickListener { onClick() }
  }

  private fun field(hint: String, value: String, inputType: Int) = EditText(this).apply {
    this.hint = hint; setText(value); this.inputType = inputType; setSingleLine()
    setTextColor(Color.WHITE); setHintTextColor(Color.parseColor("#777777"))
    setBackgroundColor(Color.parseColor("#1e1e1f")); setPadding(dp(12), dp(10), dp(12), dp(10))
    importantForAutofill = View.IMPORTANT_FOR_AUTOFILL_NO
    layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(8) }
  }

  private fun versionName(): String = try { packageManager.getPackageInfo(packageName, 0).versionName ?: "" } catch (e: Exception) { "" }

  private companion object {
    const val BG = "#272727"
    val DIM = Color.parseColor("#8a8a8a")
    const val REQ_CAPTURE = 1
    const val REQ_NOTIFICATIONS = 2
    const val REQ_MIC = 3
    const val REQ_MIC_TOGGLE = 4
    const val REQ_PICK = 5
    const val REQ_GAME = 7
    const val REQ_CAMERA = 6
  }
}
