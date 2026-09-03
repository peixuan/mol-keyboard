// SPDX-License-Identifier: Apache-2.0
import org.gradle.api.tasks.Sync

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val generatedAssets = layout.buildDirectory.dir("generated/app-assets")
val molApplicationId = providers.gradleProperty("molApplicationId")
    .getOrElse("cn.zhangpeixuan.molkeyboard")

android {
    namespace = "cn.zhangpeixuan.molkeyboard"
    compileSdk = 36
    buildToolsVersion = "36.0.0"
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = molApplicationId
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        testInstrumentationRunner = "cn.zhangpeixuan.molkeyboard.AndroidSmokeInstrumentation"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DMOL_BUILD_TESTS=OFF",
                    "-DMOL_BUILD_TOOLS=OFF",
                    "-DMOL_BUILD_DESKTOP_AUDIO=OFF",
                    "-DMOL_VALIDATE_MOBILE_SOURCES=OFF",
                )
                targets += "mol_android_audio"
            }
        }
    }

    buildTypes {
        debug {
            isJniDebuggable = true
        }
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    externalNativeBuild {
        cmake {
            path = rootProject.file("../../CMakeLists.txt")
            version = "3.31.6"
        }
    }

    sourceSets {
        getByName("main") {
            java.srcDir(rootProject.file("kotlin"))
            assets.srcDir(generatedAssets)
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
        freeCompilerArgs += "-Xjvm-default=all"
    }

    buildFeatures {
        buildConfig = true
    }

    packaging {
        jniLibs.useLegacyPackaging = false
        resources.excludes += setOf("META-INF/DEPENDENCIES", "META-INF/LICENSE*", "META-INF/NOTICE*")
    }
}

val webDist = rootProject.file("../../apps/web/dist")
val syncWebAssets by tasks.registering(Sync::class) {
    into(generatedAssets)
    from(webDist) {
        into("web")
    }
    from(rootProject.file("../../PRIVACY.md"))
    from(rootProject.file("../../THIRD_PARTY_NOTICES.md"))
    doFirst {
        check(webDist.resolve("index.html").isFile) {
            "Missing apps/web/dist. Run the documented Web production build before Gradle."
        }
    }
}

tasks.named("preBuild").configure {
    dependsOn(syncWebAssets)
}
