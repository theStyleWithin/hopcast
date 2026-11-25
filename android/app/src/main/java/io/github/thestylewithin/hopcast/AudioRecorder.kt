package io.github.thestylewithin.hopcast

import android.annotation.SuppressLint
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioPlaybackCaptureConfiguration
import android.media.AudioRecord
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.MediaMuxer
import android.media.MediaRecorder
import android.media.projection.MediaProjection
import java.io.File

/**
 * Records only sound to an M4A (AAC) file: the phone's own sound (what the game plays) and, when it is on,
 * the microphone. No picture is captured, so nothing on the screen is ever recorded.
 *
 * It runs on its own thread. If the phone can't keep up the sound is dropped, never the game's.
 */
class AudioRecorder(
  private val projection: MediaProjection?,
  private val file: File,
  private val phoneSound: Boolean,
  mic: Boolean,
) {
  @Volatile private var micWanted = mic
  @Volatile private var stopping = false
  @Volatile var failed: String? = null
    private set
  private var thread: Thread? = null

  /** Started the recorder? False when there was nothing to record (no sound source could be opened). */
  fun start(): Boolean {
    val playback = openPlayback()
    val micRecord = if (micWanted) openMic() else null
    if (playback == null && micRecord == null) return false
    val t = Thread({ run(playback, micRecord) }, "hopcast-audio")
    thread = t
    t.start()
    return true
  }

  /** Turns the microphone on or off while recording. */
  fun setMic(on: Boolean) { micWanted = on }

  /** Stops, waits for the file to be finished and returns. */
  fun stop() {
    stopping = true
    try { thread?.join(5000) } catch (e: InterruptedException) { }
  }

  @SuppressLint("MissingPermission")
  private fun openMic(): AudioRecord? = try {
    val rec = AudioRecord.Builder()
      .setAudioSource(MediaRecorder.AudioSource.MIC)
      .setAudioFormat(format(AudioFormat.CHANNEL_IN_MONO))
      .setBufferSizeInBytes(maxOf(AudioRecord.getMinBufferSize(RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT), RATE / 2) * 2)
      .build()
    if (rec.state == AudioRecord.STATE_INITIALIZED) rec else { rec.release(); null }
  } catch (e: Exception) {
    null
  }

  @SuppressLint("MissingPermission")
  private fun openPlayback(): AudioRecord? {
    val proj = projection
    if (!phoneSound || proj == null) return null
    return try {
      val config = AudioPlaybackCaptureConfiguration.Builder(proj)
        .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
        .addMatchingUsage(AudioAttributes.USAGE_GAME)
        .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
        .build()
      val rec = AudioRecord.Builder()
        .setAudioPlaybackCaptureConfig(config)
        .setAudioFormat(format(AudioFormat.CHANNEL_IN_STEREO))
        .setBufferSizeInBytes(maxOf(AudioRecord.getMinBufferSize(RATE, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT), RATE) * 2)
        .build()
      if (rec.state == AudioRecord.STATE_INITIALIZED) rec else { rec.release(); null }
    } catch (e: Exception) {
      null
    }
  }

  private fun format(channels: Int) = AudioFormat.Builder()
    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
    .setSampleRate(RATE)
    .setChannelMask(channels)
    .build()

  private fun run(playbackIn: AudioRecord?, micIn: AudioRecord?) {
    var playback = playbackIn
    var mic = micIn
    var codec: MediaCodec? = null
    var muxer: MediaMuxer? = null
    var track = -1
    var muxing = false
    try {
      val aac = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, RATE, 2).apply {
        setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC)
        setInteger(MediaFormat.KEY_BIT_RATE, BITRATE)
        setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, CHUNK_FRAMES * 4)
      }
      val enc = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC)
      codec = enc
      enc.configure(aac, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
      enc.start()
      val mux = MediaMuxer(file.absolutePath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4)
      muxer = mux
      playback?.startRecording()
      mic?.startRecording()

      val info = MediaCodec.BufferInfo()
      val stereo = ShortArray(CHUNK_FRAMES * 2)
      val monoBuf = ShortArray(CHUNK_FRAMES)
      var frames = 0L
      var endSent = false

      fun drain(wait: Boolean) {
        val c = enc
        val deadline = System.currentTimeMillis() + 5000
        while (true) {
          val i = c.dequeueOutputBuffer(info, if (wait) 10_000 else 0)
          when {
            i == MediaCodec.INFO_TRY_AGAIN_LATER -> { if (!wait || System.currentTimeMillis() > deadline) return }
            i == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
              track = mux.addTrack(c.outputFormat)
              mux.start()
              muxing = true
            }
            i >= 0 -> {
              val out = c.getOutputBuffer(i)
              if (out != null && info.size > 0 && muxing && (info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0) {
                out.position(info.offset)
                out.limit(info.offset + info.size)
                mux.writeSampleData(track, out, info)
              }
              val last = (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0
              c.releaseOutputBuffer(i, false)
              if (last) return
            }
          }
        }
      }

      while (!endSent) {
        if (stopping) {
          val i = enc.dequeueInputBuffer(10_000)
          if (i >= 0) {
            enc.queueInputBuffer(i, 0, 0, frames * 1_000_000L / RATE, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
            endSent = true
          }
          drain(false)
          continue
        }

        // the microphone can be switched on and off while recording
        if (micWanted && mic == null) {
          mic = openMic()?.also { it.startRecording() }
        } else if (!micWanted && mic != null) {
          try { mic.stop() } catch (e: Exception) { }
          mic.release()
          mic = null
        }

        // one chunk of phone sound (this read paces the loop), or silence while only the microphone is open
        var got = CHUNK_FRAMES
        if (playback != null) {
          val n = playback.read(stereo, 0, stereo.size, AudioRecord.READ_BLOCKING)
          if (n < 0) { try { playback.stop() } catch (e: Exception) { }; playback.release(); playback = null; continue }
          got = n / 2
        } else if (mic != null) {
          java.util.Arrays.fill(stereo, 0.toShort())
        } else {
          try { Thread.sleep(20) } catch (e: InterruptedException) { }
          java.util.Arrays.fill(stereo, 0.toShort())
        }
        if (got <= 0) continue
        if (playback == null) {
          // microphone only: its read paces the loop
          val m = mic
          if (m != null) {
            val n = m.read(monoBuf, 0, got, AudioRecord.READ_BLOCKING)
            if (n <= 0) continue
            for (k in 0 until n) { stereo[k * 2] = monoBuf[k]; stereo[k * 2 + 1] = monoBuf[k] }
            got = n
          }
        } else {
          val m = mic
          if (m != null) {
            // whatever the microphone has ready is added to the phone sound; it never makes the loop wait
            val n = m.read(monoBuf, 0, got, AudioRecord.READ_NON_BLOCKING)
            for (k in 0 until maxOf(n, 0)) {
              stereo[k * 2] = clip(stereo[k * 2] + monoBuf[k])
              stereo[k * 2 + 1] = clip(stereo[k * 2 + 1] + monoBuf[k])
            }
          }
        }

        // hand the chunk to the encoder
        var offset = 0
        val bytes = got * 4
        while (offset < bytes) {
          val i = enc.dequeueInputBuffer(10_000)
          if (i < 0) { drain(false); if (stopping) break; continue }
          val buf = enc.getInputBuffer(i)!!
          buf.clear()
          val take = minOf(buf.capacity(), bytes - offset)
          val frameStart = offset / 4
          for (k in 0 until take / 2) {
            val s = stereo[offset / 2 + k]
            buf.put((s.toInt() and 0xff).toByte())
            buf.put(((s.toInt() shr 8) and 0xff).toByte())
          }
          enc.queueInputBuffer(i, 0, take, (frames + frameStart) * 1_000_000L / RATE, 0)
          offset += take
          drain(false)
        }
        frames += got
        drain(false)
      }
      endSent = true
      drain(true)
    } catch (e: Exception) {
      failed = e.message ?: e.javaClass.simpleName
    } finally {
      try { playback?.stop() } catch (e: Exception) { }
      try { mic?.stop() } catch (e: Exception) { }
      try { playback?.release() } catch (e: Exception) { }
      try { mic?.release() } catch (e: Exception) { }
      try { codec?.stop() } catch (e: Exception) { }
      try { codec?.release() } catch (e: Exception) { }
      try { if (muxing) muxer?.stop() } catch (e: Exception) { if (failed == null) failed = "the file couldn't be finished" }
      try { muxer?.release() } catch (e: Exception) { }
    }
  }

  private fun clip(v: Int): Short = v.coerceIn(-32768, 32767).toShort()

  private companion object {
    const val RATE = 48000
    const val BITRATE = 160_000
    const val CHUNK_FRAMES = 960 // 20 ms
  }
}
