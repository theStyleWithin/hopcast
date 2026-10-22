pluginManagement {
  repositories {
    google()
    mavenCentral()
    gradlePluginPortal()
  }
}

@Suppress("UnstableApiUsage")
dependencyResolutionManagement {
  repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
  repositories {
    google()
    mavenCentral()
    // RootEncoder (Apache-2.0), the library that encodes and sends the stream
    maven { url = uri("https://jitpack.io") }
  }
}

rootProject.name = "HopCast"
include(":app")
