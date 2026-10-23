package io.github.thestylewithin.hopcast

import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.os.Bundle
import android.net.Uri
import android.provider.Settings
import android.text.Editable
import android.text.TextWatcher
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.BaseAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.TextView

/** Pick the apps that should never be shown on the stream or in the recording. */
class BlocklistActivity : Activity() {

  private class Entry(val label: String, val pkg: String, val icon: Drawable)

  private val all = mutableListOf<Entry>()
  private var shown = listOf<Entry>()
  private lateinit var list: ListView
  private lateinit var status: TextView
  private lateinit var enable: Button
  private var pickMode = false

  private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()

  private val adapter = object : BaseAdapter() {
    override fun getCount() = shown.size
    override fun getItem(position: Int) = shown[position]
    override fun getItemId(position: Int) = position.toLong()
    override fun getView(position: Int, convertView: View?, parent: ViewGroup?): View {
      val e = shown[position]
      val row = LinearLayout(this@BlocklistActivity).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        setPadding(dp(18), dp(8), dp(18), dp(8))
      }
      row.addView(ImageView(this@BlocklistActivity).apply { setImageDrawable(e.icon); layoutParams = LinearLayout.LayoutParams(dp(40), dp(40)) })
      val labels = LinearLayout(this@BlocklistActivity).apply {
        orientation = LinearLayout.VERTICAL
        setPadding(dp(14), 0, dp(8), 0)
        layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
      }
      labels.addView(TextView(this@BlocklistActivity).apply { text = e.label; textSize = 15f; setTextColor(Color.WHITE) })
      labels.addView(TextView(this@BlocklistActivity).apply { text = e.pkg; textSize = 11f; setTextColor(Color.parseColor("#8a8a8a")) })
      row.addView(labels)
      val box = CheckBox(this@BlocklistActivity).apply { isChecked = Studio.blocked.contains(e.pkg); isClickable = false; isFocusable = false }
      if (!pickMode) row.addView(box)
      row.setOnClickListener {
        if (pickMode) {
          setResult(RESULT_OK, Intent().putExtra("pkg", e.pkg).putExtra("label", e.label))
          finish()
          return@setOnClickListener
        }
        if (!Studio.blocked.add(e.pkg)) Studio.blocked.remove(e.pkg)
        Studio.save()
        box.isChecked = Studio.blocked.contains(e.pkg)
        refreshStatus()
      }
      return row
    }
  }

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    Studio.load(applicationContext)
    pickMode = intent.getBooleanExtra("pick", false)
    window.statusBarColor = Color.parseColor("#272727")
    window.navigationBarColor = Color.parseColor("#272727")

    val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setBackgroundColor(Color.parseColor("#272727")) }
    root.addView(TextView(this).apply {
      text = if (pickMode) "Pick your game" else "App block list"; textSize = 22f; typeface = Typeface.DEFAULT_BOLD; setTextColor(Color.WHITE)
      setPadding(dp(18), dp(24), dp(18), dp(4))
    })
    root.addView(TextView(this).apply {
      text = if (pickMode) "The stream will show only this app. Anything else, including other apps and your home screen, stays black until you are back in it." else "While you're live or recording, an app on this list goes black on the stream and in the recording the moment it is on screen, and comes back when you leave it."
      textSize = 13f; setTextColor(Color.parseColor("#bdbdbd")); setPadding(dp(18), 0, dp(18), dp(10))
    })
    status = TextView(this).apply { textSize = 13f; setTextColor(Color.parseColor("#effc35")); setPadding(dp(18), 0, dp(18), dp(6)) }
    root.addView(status)
    enable = Button(this).apply {
      isAllCaps = false
      text = "Allow usage access (one tap)"
      setOnClickListener {
        try { startActivity(Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS, Uri.parse("package:$packageName"))) }
        catch (e: Exception) { startActivity(Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS)) }
      }
      layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { setMargins(dp(18), 0, dp(18), dp(8)) }
    }
    root.addView(enable)
    root.addView(EditText(this).apply {
      hint = "Search your apps"; setSingleLine(); setTextColor(Color.WHITE); setHintTextColor(Color.parseColor("#777777"))
      setBackgroundColor(Color.parseColor("#1e1e1f")); setPadding(dp(12), dp(10), dp(12), dp(10))
      layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { setMargins(dp(18), 0, dp(18), dp(6)) }
      addTextChangedListener(object : TextWatcher {
        override fun beforeTextChanged(s: CharSequence?, st: Int, c: Int, a: Int) {}
        override fun onTextChanged(s: CharSequence?, st: Int, b: Int, c: Int) { filter(s?.toString().orEmpty()) }
        override fun afterTextChanged(s: Editable?) {}
      })
    })
    list = ListView(this).apply { adapter = this@BlocklistActivity.adapter; divider = null }
    root.addView(list, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
    setContentView(root)

    Thread {
      val pm = packageManager
      val launcher = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
      @Suppress("DEPRECATION")
      val found = pm.queryIntentActivities(launcher, 0)
        .filter { it.activityInfo.packageName != packageName }
        .map { Entry(it.loadLabel(pm).toString(), it.activityInfo.packageName, it.loadIcon(pm)) }
        .distinctBy { it.pkg }
        .sortedBy { it.label.lowercase() }
      runOnUiThread { all.clear(); all.addAll(found); filter("") }
    }.start()
  }

  override fun onResume() {
    super.onResume()
    refreshStatus()
  }

  private fun filter(query: String) {
    val q = query.trim().lowercase()
    shown = if (q.isEmpty()) all.toList() else all.filter { it.label.lowercase().contains(q) || it.pkg.lowercase().contains(q) }
    adapter.notifyDataSetChanged()
  }

  private fun refreshStatus() {
    val on = AppWatcher.allowed(this)
    if (pickMode) {
      status.text = if (on) "" else "Usage access is off, so HopCast can't tell when you're in the game yet. Allow it below, then pick."
      status.visibility = if (on) View.GONE else View.VISIBLE
      enable.visibility = if (on) View.GONE else View.VISIBLE
      return
    }
    val n = Studio.blocked.size
    status.text = (if (n == 0) "No apps on the list yet. " else "$n app${if (n == 1) "" else "s"} on the list. ") +
      if (on) "HopCast can see which app is open, so these will be hidden." else "Usage access is off, so nothing is hidden yet. It only lets HopCast see which app is open, nothing inside it."
    enable.visibility = if (on) View.GONE else View.VISIBLE
  }
}
