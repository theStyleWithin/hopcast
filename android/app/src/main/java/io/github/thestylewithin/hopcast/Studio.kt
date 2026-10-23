package io.github.thestylewithin.hopcast

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Typeface
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.util.UUID

/** Channels, scenes, sources and the app block list. Kept in one small file on the phone. */
object Studio {

  class Channel(val id: String, var name: String, var url: String, var enabled: Boolean, var icon: String)

  /**
   * One thing on the stream besides the screen. Position and width are percents of the stream's picture,
   * measured from the top left; the height follows from the picture's own shape.
   */
  class Source(
    val id: String,
    var kind: String, // "image", "gif" or "text"
    var name: String,
    var visible: Boolean,
    var file: String,
    var text: String,
    var color: Int,
    var x: Float,
    var y: Float,
    var w: Float,
    var aspect: Float, // picture width divided by height
  )

  class Scene(val id: String, var name: String, val sources: MutableList<Source> = mutableListOf())

  val channels = mutableListOf<Channel>()
  val scenes = mutableListOf<Scene>()
  var activeScene = 0
  val blocked = linkedSetOf<String>()
  /** A copy the app watcher can read from its own thread. */
  @Volatile var blockedFast: Set<String> = emptySet()

  private var loaded = false
  private lateinit var dir: File

  fun newId(): String = UUID.randomUUID().toString().take(8)

  fun scene(): Scene {
    if (scenes.isEmpty()) scenes.add(Scene(newId(), "Scene 1"))
    activeScene = activeScene.coerceIn(0, scenes.size - 1)
    return scenes[activeScene]
  }

  fun filesDir(context: Context, name: String): File = File(context.filesDir, name).also { it.mkdirs() }

  fun load(context: Context) {
    if (loaded) return
    loaded = true
    dir = context.filesDir
    val file = File(dir, "studio.json")
    if (file.exists()) {
      try {
        val root = JSONObject(file.readText())
        root.optJSONArray("channels")?.let { arr ->
          for (i in 0 until arr.length()) {
            val o = arr.getJSONObject(i)
            channels.add(Channel(o.getString("id"), o.optString("name"), o.optString("url"), o.optBoolean("enabled", true), o.optString("icon")))
          }
        }
        root.optJSONArray("scenes")?.let { arr ->
          for (i in 0 until arr.length()) {
            val o = arr.getJSONObject(i)
            val scene = Scene(o.getString("id"), o.optString("name", "Scene"))
            o.optJSONArray("sources")?.let { sa ->
              for (j in 0 until sa.length()) {
                val so = sa.getJSONObject(j)
                scene.sources.add(
                  Source(
                    so.getString("id"), so.optString("kind", "image"), so.optString("name"), so.optBoolean("visible", true),
                    so.optString("file"), so.optString("text"), so.optInt("color", Color.WHITE),
                    so.optDouble("x", 5.0).toFloat(), so.optDouble("y", 5.0).toFloat(),
                    so.optDouble("w", 30.0).toFloat(), so.optDouble("aspect", 1.0).toFloat(),
                  )
                )
              }
            }
            scenes.add(scene)
          }
        }
        activeScene = root.optInt("activeScene", 0)
        root.optJSONArray("blocked")?.let { arr -> for (i in 0 until arr.length()) blocked.add(arr.getString(i)) }
      } catch (e: Exception) {
        // a damaged file starts fresh rather than stopping the app
      }
    }
    if (channels.isEmpty()) {
      // carry over the single channel from the first test build
      val prefs = Prefs(context)
      if (prefs.url.isNotBlank()) {
        val id = newId()
        channels.add(Channel(id, prefs.name.ifBlank { "My channel" }, prefs.url, true, ""))
        prefs.setChannelKey(id, prefs.key)
      }
    }
    blockedFast = blocked.toSet()
    scene()
    save()
  }

  fun save() {
    blockedFast = blocked.toSet()
    try {
      val root = JSONObject()
      root.put("channels", JSONArray().apply {
        channels.forEach { c ->
          put(JSONObject().put("id", c.id).put("name", c.name).put("url", c.url).put("enabled", c.enabled).put("icon", c.icon))
        }
      })
      root.put("scenes", JSONArray().apply {
        scenes.forEach { s ->
          put(JSONObject().put("id", s.id).put("name", s.name).put("sources", JSONArray().apply {
            s.sources.forEach { o ->
              put(
                JSONObject().put("id", o.id).put("kind", o.kind).put("name", o.name).put("visible", o.visible)
                  .put("file", o.file).put("text", o.text).put("color", o.color)
                  .put("x", o.x.toDouble()).put("y", o.y.toDouble()).put("w", o.w.toDouble()).put("aspect", o.aspect.toDouble())
              )
            }
          }))
        }
      })
      root.put("activeScene", activeScene)
      root.put("blocked", JSONArray().apply { blocked.forEach { put(it) } })
      File(dir, "studio.json").writeText(root.toString())
    } catch (e: Exception) {
      // nothing sensible to do; the next change saves again
    }
  }

  /** Copies a picked picture into the app's own folder so it stays available. Returns the new file, or null. */
  fun importPicture(context: Context, uri: android.net.Uri, folder: String, extension: String): File? {
    return try {
      val out = File(filesDir(context, folder), newId() + "." + extension)
      context.contentResolver.openInputStream(uri)?.use { input -> FileOutputStream(out).use { input.copyTo(it) } } ?: return null
      out
    } catch (e: Exception) {
      null
    }
  }

  /** Draws a piece of text into a picture, so the preview and the stream show exactly the same thing. */
  fun renderText(context: Context, text: String, color: Int): Pair<File, Float>? {
    return try {
      val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textSize = 96f
        this.color = color
        typeface = Typeface.DEFAULT_BOLD
        setShadowLayer(8f, 0f, 3f, Color.BLACK)
      }
      val lines = text.split("\n")
      val pad = 24f
      val lineHeight = paint.fontMetrics.let { it.descent - it.ascent }
      val width = (lines.maxOf { paint.measureText(it) } + pad * 2).toInt().coerceAtLeast(48)
      val height = (lineHeight * lines.size + pad * 2).toInt()
      val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
      val canvas = Canvas(bitmap)
      lines.forEachIndexed { i, line -> canvas.drawText(line, pad, pad - paint.fontMetrics.ascent + lineHeight * i, paint) }
      val file = File(filesDir(context, "sources"), newId() + ".png")
      FileOutputStream(file).use { bitmap.compress(Bitmap.CompressFormat.PNG, 100, it) }
      Pair(file, width.toFloat() / height)
    } catch (e: Exception) {
      null
    }
  }

  fun decode(path: String, maxSide: Int = 768): Bitmap? {
    return try {
      val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
      BitmapFactory.decodeFile(path, bounds)
      var sample = 1
      while (bounds.outWidth / sample > maxSide * 2 || bounds.outHeight / sample > maxSide * 2) sample *= 2
      BitmapFactory.decodeFile(path, BitmapFactory.Options().apply { inSampleSize = sample })
    } catch (e: Exception) {
      null
    }
  }
}
