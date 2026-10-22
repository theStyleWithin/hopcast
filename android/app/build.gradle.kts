plugins {
  id("com.android.application")
}

// The version comes from hopcast/VERSION (for example 0.1.15.0), the same file the desktop builds use.
val hopcastVersion = file("../../hopcast/VERSION").readText().trim()
val versionParts = hopcastVersion.split(".").map { it.toInt() }
val hopcastVersionName = versionParts.take(3).joinToString(".")
val hopcastVersionCode = versionParts[0] * 10000 + versionParts[1] * 100 + versionParts[2]

android {
  namespace = "io.github.thestylewithin.hopcast"
  compileSdk = 36

  defaultConfig {
    applicationId = "io.github.thestylewithin.hopcast"
    minSdk = 29 // Android 10: needed to capture the game's own sound
    targetSdk = 34
    versionCode = hopcastVersionCode
    versionName = hopcastVersionName
  }

  // A public test key (see android/README.md). Test builds only; it is not a release key.
  signingConfigs {
    create("test") {
      storeFile = file("../hopcast-test.keystore")
      storePassword = "hopcasttest"
      keyAlias = "hopcasttest"
      keyPassword = "hopcasttest"
    }
  }

  buildTypes {
    release {
      isMinifyEnabled = false
      signingConfig = signingConfigs.getByName("test")
    }
  }

  compileOptions {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
  }
}

kotlin {
  jvmToolchain(17)
}

dependencies {
  implementation("com.github.pedroSG94.RootEncoder:library:2.8.1")
}
