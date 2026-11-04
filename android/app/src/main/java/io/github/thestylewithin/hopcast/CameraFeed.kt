package io.github.thestylewithin.hopcast

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.ImageFormat
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.YuvImage
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.hardware.display.DisplayManager
import android.media.Image
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.view.Display
import android.view.Surface
import java.io.ByteArrayOutputStream

/**
 * One phone camera, delivered as small upright pictures (4:3, about 15 a second) that the stream draws over your screen.
 * Pictures instead of a raw camera texture keep the rotation and the front/back switch simple.
 */
class CameraFeed(private val context: Context, val front: Boolean, private val onFrame: (Bitmap) -> Unit) {

  private val manager = context.getSystemService(CameraManager::class.java)
  private val thread = HandlerThread("hopcast-camera-" + (if (front) "front" else "back")).apply { start() }
  private val handler = Handler(thread.looper)
  private var device: CameraDevice? = null
  private var session: CameraCaptureSession? = null
  private var reader: ImageReader? = null
  private var sensorOrientation = 90
  private var lastMs = 0L
  @Volatile private var closed = false

  /** Opens the camera. Returns false when this phone has no such camera or HopCast isn't allowed to use it. */
  fun open(): Boolean {
    if (context.checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) return false
    val facing = if (front) CameraCharacteristics.LENS_FACING_FRONT else CameraCharacteristics.LENS_FACING_BACK
    val id = try {
      manager.cameraIdList.firstOrNull { manager.getCameraCharacteristics(it).get(CameraCharacteristics.LENS_FACING) == facing }
    } catch (e: Exception) {
      null
    } ?: return false
    sensorOrientation = manager.getCameraCharacteristics(id).get(CameraCharacteristics.SENSOR_ORIENTATION) ?: 90
    val imageReader = ImageReader.newInstance(WIDTH, HEIGHT, ImageFormat.YUV_420_888, 2)
    imageReader.setOnImageAvailableListener({ r ->
      val image = r.acquireLatestImage() ?: return@setOnImageAvailableListener
      try {
        val now = System.currentTimeMillis()
        if (!closed && now - lastMs >= FRAME_GAP_MS) {
          lastMs = now
          upright(image)?.let(onFrame)
        }
      } finally {
        image.close()
      }
    }, handler)
    reader = imageReader
    return try {
      @Suppress("MissingPermission")
      manager.openCamera(id, object : CameraDevice.StateCallback() {
        override fun onOpened(camera: CameraDevice) {
          if (closed) { camera.close(); return }
          device = camera
          try {
            @Suppress("DEPRECATION")
            camera.createCaptureSession(listOf(imageReader.surface), object : CameraCaptureSession.StateCallback() {
              override fun onConfigured(s: CameraCaptureSession) {
                if (closed) { s.close(); return }
                session = s
                try {
                  val request = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW).apply { addTarget(imageReader.surface) }.build()
                  s.setRepeatingRequest(request, null, handler)
                } catch (e: Exception) { }
              }
              override fun onConfigureFailed(s: CameraCaptureSession) {}
            }, handler)
          } catch (e: Exception) { }
        }
        override fun onDisconnected(camera: CameraDevice) { camera.close() }
        override fun onError(camera: CameraDevice, error: Int) { camera.close() }
      }, handler)
      true
    } catch (e: Exception) {
      false
    }
  }

  fun close() {
    closed = true
    try { session?.close() } catch (e: Exception) { }
    try { device?.close() } catch (e: Exception) { }
    try { reader?.close() } catch (e: Exception) { }
    session = null; device = null; reader = null
    thread.quitSafely()
  }

  private fun displayDegrees(): Int {
    val display = context.getSystemService(DisplayManager::class.java).getDisplay(Display.DEFAULT_DISPLAY)
    return when (display?.rotation) { Surface.ROTATION_90 -> 90; Surface.ROTATION_180 -> 180; Surface.ROTATION_270 -> 270; else -> 0 }
  }

  /** Turns a camera frame into an upright 4:3 picture, whichever way the phone is held. */
  private fun upright(image: Image): Bitmap? {
    return try {
      val nv21 = nv21(image)
      val jpeg = ByteArrayOutputStream()
      YuvImage(nv21, ImageFormat.NV21, image.width, image.height, null).compressToJpeg(Rect(0, 0, image.width, image.height), 80, jpeg)
      val raw = BitmapFactory.decodeByteArray(jpeg.toByteArray(), 0, jpeg.size()) ?: return null
      val degrees = if (front) (sensorOrientation + displayDegrees()) % 360 else (sensorOrientation - displayDegrees() + 360) % 360
      val turned = Bitmap.createBitmap(raw, 0, 0, raw.width, raw.height, Matrix().apply { postRotate(degrees.toFloat()) }, true)
      val out = Bitmap.createBitmap(OUT_WIDTH, OUT_HEIGHT, Bitmap.Config.ARGB_8888)
      val scale = Math.max(OUT_WIDTH / turned.width.toFloat(), OUT_HEIGHT / turned.height.toFloat())
      val w = turned.width * scale
      val h = turned.height * scale
      Canvas(out).drawBitmap(turned, null, android.graphics.RectF((OUT_WIDTH - w) / 2f, (OUT_HEIGHT - h) / 2f, (OUT_WIDTH + w) / 2f, (OUT_HEIGHT + h) / 2f), Paint(Paint.FILTER_BITMAP_FLAG))
      out
    } catch (e: Exception) {
      null
    }
  }

  private fun nv21(image: Image): ByteArray {
    val w = image.width
    val h = image.height
    val y = image.planes[0]
    val u = image.planes[1]
    val v = image.planes[2]
    val out = ByteArray(w * h * 3 / 2)
    var pos = 0
    for (row in 0 until h) {
      y.buffer.position(row * y.rowStride)
      y.buffer.get(out, pos, w)
      pos += w
    }
    for (row in 0 until h / 2) {
      for (col in 0 until w / 2) {
        out[pos++] = v.buffer.get(row * v.rowStride + col * v.pixelStride)
        out[pos++] = u.buffer.get(row * u.rowStride + col * u.pixelStride)
      }
    }
    return out
  }

  companion object {
    private const val WIDTH = 640
    private const val HEIGHT = 480
    private const val OUT_WIDTH = 480
    private const val OUT_HEIGHT = 360
    private const val FRAME_GAP_MS = 66L

    /** Whether this phone can run the front and back cameras at the same time. */
    fun bothAtOnce(context: Context): Boolean {
      return try {
        val manager = context.getSystemService(CameraManager::class.java)
        val front = manager.cameraIdList.filter { manager.getCameraCharacteristics(it).get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_FRONT }
        val back = manager.cameraIdList.filter { manager.getCameraCharacteristics(it).get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_BACK }
        manager.concurrentCameraIds.any { set -> front.any { it in set } && back.any { it in set } }
      } catch (e: Exception) {
        false
      }
    }

    fun hasFront(context: Context): Boolean = context.packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_FRONT)
    fun hasBack(context: Context): Boolean = context.packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA)
  }
}
