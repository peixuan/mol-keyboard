// SPDX-License-Identifier: Apache-2.0
package cn.zhangpeixuan.molkeyboard

import android.Manifest
import android.app.Activity
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager
import android.content.res.AssetManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.view.KeyEvent
import android.view.ViewGroup
import android.webkit.CookieManager
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import java.io.ByteArrayInputStream
import java.io.FileNotFoundException

class MainActivity : Activity() {
    private lateinit var webView: WebView
    private var audioService: AudioForegroundService? = null
    private var bound = false

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            audioService = (binder as? AudioForegroundService.LocalBinder)?.service
            bound = audioService != null
            audioService?.onUiForegrounded()
            webView.evaluateJavascript("window.dispatchEvent(new Event('molnativeavailable'))", null)
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            audioService = null
            bound = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WebView.setWebContentsDebuggingEnabled(
            applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE != 0,
        )
        webView = WebView(this).apply {
            setBackgroundColor(Color.rgb(242, 239, 231))
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            )
            settings.javaScriptEnabled = true
            settings.domStorageEnabled = true
            settings.allowContentAccess = false
            settings.allowFileAccess = false
            settings.mediaPlaybackRequiresUserGesture = true
            settings.mixedContentMode = android.webkit.WebSettings.MIXED_CONTENT_NEVER_ALLOW
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) settings.safeBrowsingEnabled = true
            webViewClient = object : WebViewClient() {
                override fun shouldOverrideUrlLoading(view: WebView?, request: WebResourceRequest?): Boolean {
                    val url = request?.url?.toString() ?: return true
                    return !url.startsWith(APP_ASSET_ROOT)
                }

                override fun shouldInterceptRequest(
                    view: WebView?,
                    request: WebResourceRequest?,
                ): WebResourceResponse? {
                    val url = request?.url ?: return forbiddenResponse()
                    if (url.scheme != APP_SCHEME || url.host != APP_HOST) return forbiddenResponse()
                    val path = url.path?.removePrefix(APP_PATH_PREFIX) ?: return notFoundResponse()
                    if (path.isEmpty() || path.contains("..") || path.contains('\\')) {
                        return notFoundResponse()
                    }
                    return try {
                        WebResourceResponse(
                            mimeType(path),
                            if (isTextAsset(path)) "UTF-8" else null,
                            assets.open("web/$path", AssetManager.ACCESS_STREAMING),
                        )
                    } catch (_: FileNotFoundException) {
                        notFoundResponse()
                    }
                }
            }
            addJavascriptInterface(NativeWebBridge(this@MainActivity) { audioService }, BRIDGE_NAME)
        }
        CookieManager.getInstance().setAcceptCookie(false)
        setContentView(webView)
        bindService(Intent(this, AudioForegroundService::class.java), serviceConnection, Context.BIND_AUTO_CREATE)
        webView.loadUrl(APP_INDEX)
    }

    override fun onResume() {
        super.onResume()
        audioService?.onUiForegrounded()
    }

    override fun onPause() {
        audioService?.onUiBackgrounded()
        super.onPause()
    }

    override fun onDestroy() {
        webView.removeJavascriptInterface(BRIDGE_NAME)
        webView.stopLoading()
        webView.destroy()
        if (bound) unbindService(serviceConnection)
        bound = false
        audioService = null
        super.onDestroy()
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val note = NOTE_BY_KEY_CODE[event.keyCode] ?: return super.dispatchKeyEvent(event)
        val gesture = HARDWARE_GESTURE_PREFIX or
            ((event.deviceId.toLong() and 0xFFFFL) shl 16) or
            (event.keyCode.toLong() and 0xFFFFL)
        val service = audioService ?: return super.dispatchKeyEvent(event)
        return when (event.action) {
            KeyEvent.ACTION_DOWN -> {
                if (event.repeatCount == 0) {
                    service.submitControl(
                        NativeCommands.NOTE_ON,
                        gesture,
                        note,
                        0,
                        0,
                        0,
                        0.82f,
                        0f,
                    ) == 0
                } else {
                    true
                }
            }
            KeyEvent.ACTION_UP -> {
                service.submitControl(
                    NativeCommands.NOTE_OFF,
                    gesture,
                    note,
                    0,
                    0,
                    0,
                    0f,
                    0f,
                ) == 0
            }
            else -> super.dispatchKeyEvent(event)
        }
    }

    fun startNativeAudio() {
        runOnUiThread {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
                checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
            ) {
                requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), NOTIFICATION_PERMISSION_REQUEST)
                return@runOnUiThread
            }
            startAudioService()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray,
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == NOTIFICATION_PERMISSION_REQUEST) startAudioService()
    }

    fun stopNativeAudio() {
        startService(
            Intent(this, AudioForegroundService::class.java).setAction(AudioForegroundService.ACTION_STOP),
        )
    }

    private fun startAudioService() {
        startForegroundService(
            Intent(this, AudioForegroundService::class.java).setAction(AudioForegroundService.ACTION_START),
        )
    }

    companion object {
        private const val BRIDGE_NAME = "MolKeyboardNative"
        private const val APP_SCHEME = "https"
        private const val APP_HOST = "appassets.androidplatform.net"
        private const val APP_PATH_PREFIX = "/assets/web/"
        private const val APP_ASSET_ROOT = "$APP_SCHEME://$APP_HOST$APP_PATH_PREFIX"
        private const val APP_INDEX = "${APP_ASSET_ROOT}index.html"
        private const val NOTIFICATION_PERMISSION_REQUEST = 0x4D4F
        private const val HARDWARE_GESTURE_PREFIX = 1L shl 52
        private val NOTE_BY_KEY_CODE = mapOf(
            KeyEvent.KEYCODE_Z to 60,
            KeyEvent.KEYCODE_S to 61,
            KeyEvent.KEYCODE_X to 62,
            KeyEvent.KEYCODE_D to 63,
            KeyEvent.KEYCODE_C to 64,
            KeyEvent.KEYCODE_V to 65,
            KeyEvent.KEYCODE_G to 66,
            KeyEvent.KEYCODE_B to 67,
            KeyEvent.KEYCODE_H to 68,
            KeyEvent.KEYCODE_N to 69,
            KeyEvent.KEYCODE_J to 70,
            KeyEvent.KEYCODE_M to 71,
            KeyEvent.KEYCODE_Q to 72,
            KeyEvent.KEYCODE_2 to 73,
            KeyEvent.KEYCODE_W to 74,
            KeyEvent.KEYCODE_3 to 75,
            KeyEvent.KEYCODE_E to 76,
            KeyEvent.KEYCODE_R to 77,
            KeyEvent.KEYCODE_5 to 78,
            KeyEvent.KEYCODE_T to 79,
            KeyEvent.KEYCODE_6 to 80,
            KeyEvent.KEYCODE_Y to 81,
            KeyEvent.KEYCODE_7 to 82,
            KeyEvent.KEYCODE_U to 83,
            KeyEvent.KEYCODE_I to 84,
            KeyEvent.KEYCODE_9 to 85,
            KeyEvent.KEYCODE_O to 86,
            KeyEvent.KEYCODE_0 to 87,
            KeyEvent.KEYCODE_P to 88,
            KeyEvent.KEYCODE_LEFT_BRACKET to 89,
        )

        private fun isTextAsset(path: String): Boolean =
            path.endsWith(".css") || path.endsWith(".html") || path.endsWith(".js") ||
                path.endsWith(".json") || path.endsWith(".map") || path.endsWith(".svg") ||
                path.endsWith(".webmanifest")

        private fun mimeType(path: String): String = when {
            path.endsWith(".css") -> "text/css"
            path.endsWith(".html") -> "text/html"
            path.endsWith(".js") -> "application/javascript"
            path.endsWith(".json") || path.endsWith(".map") -> "application/json"
            path.endsWith(".svg") -> "image/svg+xml"
            path.endsWith(".webmanifest") -> "application/manifest+json"
            path.endsWith(".wasm") -> "application/wasm"
            path.endsWith(".png") -> "image/png"
            else -> "application/octet-stream"
        }

        private fun notFoundResponse(): WebResourceResponse = errorResponse(404, "Not Found")

        private fun forbiddenResponse(): WebResourceResponse = errorResponse(403, "Forbidden")

        private fun errorResponse(status: Int, reason: String): WebResourceResponse = WebResourceResponse(
            "text/plain",
            "UTF-8",
            status,
            reason,
            mapOf("Cache-Control" to "no-store"),
            ByteArrayInputStream(reason.toByteArray(Charsets.UTF_8)),
        )
    }
}
