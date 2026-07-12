plugins {
    id("com.android.application")
}

android {
    namespace = "org.freeaoe"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "org.freeaoe"
        minSdk = 28
        targetSdk = 36
        versionCode = 1
        versionName = "0.1"

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DUSE_SDL2=ON",
                    "-DANDROID_STL=c++_shared"
                )
                cppFlags += "-std=c++20"
            }
        }
        ndk {
            abiFilters += "arm64-v8a"
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}
