package io.github.thestylewithin.hopcast

import android.content.Context
import android.graphics.Bitmap
import android.opengl.GLES20
import android.opengl.Matrix
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import com.pedro.encoder.input.gl.render.filters.BaseFilterRender
import com.pedro.encoder.utils.gl.GlUtil
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * A filter that changes nothing. It sits first in the chain, so it sees your screen before any black boxes are drawn,
 * and now and then hands a small grey copy of it to the tracker. It can also give out one full picture on request.
 * Both stay on the phone.
 */
class ProbeFilterRender(
  /** Gets a small grey picture (top row first): grey bytes, its width, its height, and how many full-size pixels one grey pixel stands for. */
  private val onGrey: (ByteArray, Int, Int, Int) -> Unit,
) : BaseFilterRender() {

  private val vertexData = floatArrayOf(
    -1f, -1f, 0f, 0f, 0f,
    1f, -1f, 0f, 1f, 0f,
    -1f, 1f, 0f, 0f, 1f,
    1f, 1f, 0f, 1f, 1f,
  )
  private var program = -1
  private var aPosition = -1
  private var aTexture = -1
  private var uMvp = -1
  private var uSt = -1
  private var uSampler = -1
  private var lastMs = 0L
  private var buffer: ByteBuffer? = null
  private val main = Handler(Looper.getMainLooper())
  @Volatile private var snapshotWanted: ((Bitmap) -> Unit)? = null
  @Volatile var intervalMs = 110L
  /** How many full-size pixels make one grey pixel. */
  var step = STEP
  private var rgba: ByteArray? = null

  init {
    squareVertex = ByteBuffer.allocateDirect(vertexData.size * FLOAT_SIZE_BYTES).order(ByteOrder.nativeOrder()).asFloatBuffer()
    squareVertex.put(vertexData).position(0)
    Matrix.setIdentityM(MVPMatrix, 0)
    Matrix.setIdentityM(STMatrix, 0)
  }

  /** Asks for one full-size picture of your screen from the next frame. The callback runs on the main thread. */
  fun requestSnapshot(callback: (Bitmap) -> Unit) { snapshotWanted = callback }

  override fun initGlFilter(context: Context) {
    val vertex = "uniform mat4 uMVPMatrix;\nuniform mat4 uSTMatrix;\nattribute vec4 aPosition;\nattribute vec4 aTextureCoord;\nvarying vec2 vTextureCoord;\n" +
      "void main() {\n  gl_Position = uMVPMatrix * aPosition;\n  vTextureCoord = (uSTMatrix * aTextureCoord).xy;\n}\n"
    val fragment = "precision mediump float;\nvarying vec2 vTextureCoord;\nuniform sampler2D uSampler;\n" +
      "void main() {\n  gl_FragColor = texture2D(uSampler, vTextureCoord);\n}\n"
    program = GlUtil.createProgram(vertex, fragment)
    aPosition = GLES20.glGetAttribLocation(program, "aPosition")
    aTexture = GLES20.glGetAttribLocation(program, "aTextureCoord")
    uMvp = GLES20.glGetUniformLocation(program, "uMVPMatrix")
    uSt = GLES20.glGetUniformLocation(program, "uSTMatrix")
    uSampler = GLES20.glGetUniformLocation(program, "uSampler")
  }

  override fun drawFilter() {
    GLES20.glUseProgram(program)
    squareVertex.position(SQUARE_VERTEX_DATA_POS_OFFSET)
    GLES20.glVertexAttribPointer(aPosition, 3, GLES20.GL_FLOAT, false, SQUARE_VERTEX_DATA_STRIDE_BYTES, squareVertex)
    GLES20.glEnableVertexAttribArray(aPosition)
    squareVertex.position(SQUARE_VERTEX_DATA_UV_OFFSET)
    GLES20.glVertexAttribPointer(aTexture, 2, GLES20.GL_FLOAT, false, SQUARE_VERTEX_DATA_STRIDE_BYTES, squareVertex)
    GLES20.glEnableVertexAttribArray(aTexture)
    GLES20.glUniformMatrix4fv(uMvp, 1, false, MVPMatrix, 0)
    GLES20.glUniformMatrix4fv(uSt, 1, false, STMatrix, 0)
    GLES20.glUniform1i(uSampler, 0)
    GLES20.glActiveTexture(GLES20.GL_TEXTURE0)
    GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, previousTexId)
  }

  override fun disableResources() {
    GlUtil.disableResources(aTexture, aPosition)
  }

  override fun release() {
    GLES20.glDeleteProgram(program)
  }

  override fun draw() {
    super.draw()
    val wantSnapshot = snapshotWanted
    val now = SystemClock.uptimeMillis()
    if (wantSnapshot == null && now - lastMs < intervalMs) return
    lastMs = now
    val w = getWidth()
    val h = getHeight()
    if (w <= 0 || h <= 0) return
    var buf = buffer
    if (buf == null || buf.capacity() != w * h * 4) {
      buf = ByteBuffer.allocateDirect(w * h * 4).order(ByteOrder.nativeOrder())
      buffer = buf
    }
    buf.rewind()
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, getRenderHandler().getFboId()[0])
    GLES20.glReadPixels(0, 0, w, h, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, buf)
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0)

    if (wantSnapshot != null) {
      snapshotWanted = null
      val pixels = IntArray(w * h)
      buf.rewind()
      for (row in 0 until h) {
        val dst = (h - 1 - row) * w // the buffer starts at the bottom row
        for (col in 0 until w) {
          val r = buf.get().toInt() and 0xff
          val g = buf.get().toInt() and 0xff
          val b = buf.get().toInt() and 0xff
          buf.get()
          pixels[dst + col] = (0xff shl 24) or (r shl 16) or (g shl 8) or b
        }
      }
      val bitmap = Bitmap.createBitmap(pixels, w, h, Bitmap.Config.ARGB_8888)
      main.post { wantSnapshot(bitmap) }
    }

    // every grey pixel is the average of a step x step block, so it matches the tag pictures (cut on the same grid)
    // even when an icon sits a pixel or two off from where it was tagged
    var bytes = rgba
    if (bytes == null || bytes.size != w * h * 4) { bytes = ByteArray(w * h * 4); rgba = bytes }
    buf.rewind()
    buf.get(bytes)
    val s = step
    val gw = w / s
    val gh = h / s
    val grey = ByteArray(gw * gh)
    val area = s * s
    for (gy in 0 until gh) {
      for (gx in 0 until gw) {
        var sum = 0
        for (dy in 0 until s) {
          var i = ((h - 1 - (gy * s + dy)) * w + gx * s) * 4 // top row first
          for (dx in 0 until s) {
            sum += ((bytes[i].toInt() and 0xff) * 77 + (bytes[i + 1].toInt() and 0xff) * 150 + (bytes[i + 2].toInt() and 0xff) * 29) shr 8
            i += 4
          }
        }
        grey[gy * gw + gx] = (sum / area).toByte()
      }
    }
    onGrey(grey, gw, gh, s)
  }

  companion object {
    /** Full-size pixels per grey pixel. Tag pictures are cut on this grid so they line up with the grey pictures. */
    const val STEP = 4
  }
}
