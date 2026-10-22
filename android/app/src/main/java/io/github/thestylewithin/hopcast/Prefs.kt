package io.github.thestylewithin.hopcast

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** Settings kept on the phone only. The stream key is stored encrypted with a key the phone's keystore holds. */
class Prefs(context: Context) {
  private val sp = context.getSharedPreferences("hopcast", Context.MODE_PRIVATE)

  var name: String
    get() = sp.getString("name", "") ?: ""
    set(v) = sp.edit().putString("name", v).apply()

  var url: String
    get() = sp.getString("url", "") ?: ""
    set(v) = sp.edit().putString("url", v).apply()

  var gameSound: Boolean
    get() = sp.getBoolean("gameSound", true)
    set(v) = sp.edit().putBoolean("gameSound", v).apply()

  /** Off until the user turns it on. */
  var mic: Boolean
    get() = sp.getBoolean("mic", false)
    set(v) = sp.edit().putBoolean("mic", v).apply()

  var bubble: Boolean
    get() = sp.getBoolean("bubble", true)
    set(v) = sp.edit().putBoolean("bubble", v).apply()

  /** 0 = 720p 30 fps, 1 = 1080p 30 fps, 2 = 720p 60 fps */
  var quality: Int
    get() = sp.getInt("quality", 0)
    set(v) = sp.edit().putInt("quality", v).apply()

  /** 0 = auto (follows the game), 1 = portrait, 2 = landscape */
  var shape: Int
    get() = sp.getInt("shape", 0)
    set(v) = sp.edit().putInt("shape", v).apply()

  /** The one-time notice about what goes out live has been shown. */
  var warned: Boolean
    get() = sp.getBoolean("warned", false)
    set(v) = sp.edit().putBoolean("warned", v).apply()

  var key: String
    get() = decrypt(sp.getString("key", "") ?: "")
    set(v) = sp.edit().putString("key", encrypt(v)).apply()

  private fun secretKey(): SecretKey {
    val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
    (store.getKey(KEY_ALIAS, null) as? SecretKey)?.let { return it }
    val gen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
    gen.init(
      KeyGenParameterSpec.Builder(KEY_ALIAS, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
        .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
        .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
        .build()
    )
    return gen.generateKey()
  }

  private fun encrypt(plain: String): String {
    if (plain.isEmpty()) return ""
    return try {
      val cipher = Cipher.getInstance("AES/GCM/NoPadding")
      cipher.init(Cipher.ENCRYPT_MODE, secretKey())
      val data = cipher.doFinal(plain.toByteArray(Charsets.UTF_8))
      Base64.encodeToString(cipher.iv, Base64.NO_WRAP) + ":" + Base64.encodeToString(data, Base64.NO_WRAP)
    } catch (e: Exception) {
      ""
    }
  }

  private fun decrypt(stored: String): String {
    val parts = stored.split(":")
    if (parts.size != 2) return ""
    return try {
      val cipher = Cipher.getInstance("AES/GCM/NoPadding")
      cipher.init(Cipher.DECRYPT_MODE, secretKey(), GCMParameterSpec(128, Base64.decode(parts[0], Base64.NO_WRAP)))
      String(cipher.doFinal(Base64.decode(parts[1], Base64.NO_WRAP)), Charsets.UTF_8)
    } catch (e: Exception) {
      ""
    }
  }

  private companion object {
    const val KEY_ALIAS = "hopcast-stream-key"
  }
}
