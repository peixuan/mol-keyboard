// SPDX-License-Identifier: Apache-2.0
package cn.zhangpeixuan.molkeyboard

import android.app.Activity
import android.app.Instrumentation
import android.content.Intent
import android.os.Bundle
import android.os.ParcelFileDescriptor
import android.os.SystemClock
import android.view.View
import android.view.ViewGroup
import android.webkit.WebView
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

class AndroidSmokeInstrumentation : Instrumentation() {
    override fun onCreate(arguments: Bundle?) {
        super.onCreate(arguments)
        start()
    }

    override fun onStart() {
        Thread(::runSmoke, "mol-android-smoke").start()
    }

    private fun runSmoke() {
        val results = Bundle()
        var activity: MainActivity? = null
        try {
            activity = startActivitySync(
                Intent(targetContext, MainActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            ) as MainActivity
            waitForIdleSync()
            val webView = requireNotNull(findWebView(activity.window.decorView)) {
                "The packaged application did not create its WebView"
            }
            waitForWebApplication(webView)

            val page = JSONObject(
                evaluate(
                    webView,
                    "JSON.stringify({title:document.title,native:typeof MolKeyboardNative?.dispatch})",
                ),
            )
            check(page.getString("title").startsWith("MoL Keyboard"))
            check(page.getString("native") == "function")

            evaluate(
                webView,
                "document.querySelector('[data-action=start]').click(); 'started'",
            )
            val active = waitForActiveRuntime(webView)
            check(active.getBoolean("active"))
            check(active.getInt("sampleRate") in 8_000..384_000)
            check(active.getInt("framesPerBurst") > 0)

            val noteOn = bridgeRequest(
                webView,
                """{"version":1,"method":"command.submit","params":{"type":1,"gesture":4242,"i0":60,"i1":0,"i2":0,"i3":0,"f0":0.8,"f1":0}}""",
            )
            check(noteOn.getBoolean("ok"))
            SystemClock.sleep(150)
            val noteOff = bridgeRequest(
                webView,
                """{"version":1,"method":"command.submit","params":{"type":2,"gesture":4242,"i0":60,"i1":0,"i2":0,"i3":0,"f0":0,"f1":0}}""",
            )
            check(noteOff.getBoolean("ok"))
            SystemClock.sleep(150)
            val rendered = bridgeRequest(
                webView,
                """{"version":1,"method":"runtime.status","params":{}}""",
            )
            check(rendered.getLong("callbackCount") > 0L)
            check(rendered.getLong("renderedFrames") > 0L)
            check(rendered.getInt("renderFailures") == 0)
            check(rendered.getInt("nonFiniteSamples") == 0)

            val backgroundEvidence =
                verifyBackgroundLifecycle(activity, webView, rendered.getLong("callbackCount"))

            results.putString("audioApi", rendered.getInt("audioApi").toString())
            results.putString("callbacks", rendered.getLong("callbackCount").toString())
            results.putString("frames", rendered.getLong("renderedFrames").toString())
            results.putString("sampleRate", rendered.getInt("sampleRate").toString())
            results.putString("backgroundCallbacks", backgroundEvidence.backgroundCallbacks.toString())
            results.putString("lockedCallbacks", backgroundEvidence.lockedCallbacks.toString())
            results.putString("idleBackgroundStopped", "true")
            finish(Activity.RESULT_OK, results)
        } catch (error: Throwable) {
            results.putString("failure", error.stackTraceToString().take(MAXIMUM_FAILURE_CHARS))
            finish(Activity.RESULT_CANCELED, results)
        } finally {
            activity?.runOnUiThread { activity.finish() }
        }
    }

    private fun verifyBackgroundLifecycle(
        activity: MainActivity,
        webView: WebView,
        initialCallbackCount: Long,
    ): BackgroundEvidence {
        val targetPackage = targetContext.packageName
        check(
            bridgeRequest(
                webView,
                controlRequest(NativeCommands.SET_METRONOME, integer0 = 1),
            ).getBoolean("ok"),
        )
        check(
            bridgeRequest(
                webView,
                controlRequest(NativeCommands.TRANSPORT_START),
            ).getBoolean("ok"),
        )
        onActivityThread(activity) { check(activity.moveTaskToBack(true)) }
        SystemClock.sleep(BACKGROUND_SETTLE_MS)

        val background = bridgeRequest(
            webView,
            """{"version":1,"method":"runtime.status","params":{}}""",
        )
        check(background.getBoolean("active"))
        check(background.getLong("callbackCount") > initialCallbackCount)
        val runningService = shell("dumpsys activity services $targetPackage")
        check(runningService.contains("isForeground=true"))
        check(runningService.contains("foregroundId=$NOTIFICATION_ID"))

        shell("input keyevent KEYCODE_SLEEP")
        SystemClock.sleep(BACKGROUND_SETTLE_MS)
        val locked = bridgeRequest(
            webView,
            """{"version":1,"method":"runtime.status","params":{}}""",
        )
        check(locked.getBoolean("active"))
        check(locked.getLong("callbackCount") > background.getLong("callbackCount"))

        shell("input keyevent KEYCODE_WAKEUP")
        shell("wm dismiss-keyguard")
        shell("am start -W -n $targetPackage/.MainActivity")
        waitForIdleSync()
        check(
            bridgeRequest(
                webView,
                controlRequest(NativeCommands.SET_METRONOME, integer0 = 0),
            ).getBoolean("ok"),
        )
        check(
            bridgeRequest(
                webView,
                controlRequest(NativeCommands.TRANSPORT_STOP),
            ).getBoolean("ok"),
        )
        onActivityThread(activity) { check(activity.moveTaskToBack(true)) }
        val deadline = SystemClock.uptimeMillis() + AUDIO_TIMEOUT_MS
        var inactive = false
        while (SystemClock.uptimeMillis() < deadline) {
            val status = bridgeRequest(
                webView,
                """{"version":1,"method":"runtime.status","params":{}}""",
            )
            inactive = !status.optBoolean("active")
            if (inactive) break
            SystemClock.sleep(POLL_INTERVAL_MS)
        }
        check(inactive) { "An idle background runtime remained active" }
        val stoppedService = shell("dumpsys activity services $targetPackage")
        check(!stoppedService.contains("isForeground=true"))
        return BackgroundEvidence(
            background.getLong("callbackCount"),
            locked.getLong("callbackCount"),
        )
    }

    private fun controlRequest(commandType: Int, integer0: Int = 0): String =
        """{"version":1,"method":"command.submit","params":{"type":$commandType,"gesture":0,"i0":$integer0,"i1":0,"i2":0,"i3":0,"f0":0,"f1":0}}"""

    private fun onActivityThread(activity: Activity, action: () -> Unit) {
        val latch = CountDownLatch(1)
        val failure = AtomicReference<Throwable>()
        activity.runOnUiThread {
            runCatching(action).onFailure(failure::set)
            latch.countDown()
        }
        check(latch.await(EVALUATION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            "Activity operation timed out"
        }
        failure.get()?.let { throw it }
    }

    private fun shell(command: String): String =
        ParcelFileDescriptor.AutoCloseInputStream(uiAutomation.executeShellCommand(command)).use {
            it.bufferedReader().readText()
        }

    private fun waitForWebApplication(webView: WebView) {
        val deadline = SystemClock.uptimeMillis() + WEB_TIMEOUT_MS
        while (SystemClock.uptimeMillis() < deadline) {
            val ready = evaluate(
                webView,
                "document.readyState === 'complete' && document.querySelector('[data-action=start]') !== null",
            )
            if (ready == "true") return
            SystemClock.sleep(POLL_INTERVAL_MS)
        }
        error("The packaged Web application did not become ready")
    }

    private fun waitForActiveRuntime(webView: WebView): JSONObject {
        val deadline = SystemClock.uptimeMillis() + AUDIO_TIMEOUT_MS
        var last = JSONObject()
        while (SystemClock.uptimeMillis() < deadline) {
            runCatching {
                bridgeRequest(
                    webView,
                    """{"version":1,"method":"runtime.status","params":{}}""",
                )
            }.onSuccess {
                last = it
                if (last.optBoolean("active")) return last
            }
            SystemClock.sleep(POLL_INTERVAL_MS)
        }
        error("The native audio runtime did not start: $last")
    }

    private fun bridgeRequest(webView: WebView, request: String): JSONObject {
        val expression =
            "MolKeyboardNative.dispatch(${JSONObject.quote(request)})"
        return JSONObject(evaluate(webView, expression))
    }

    private fun evaluate(webView: WebView, expression: String): String {
        val latch = CountDownLatch(1)
        val result = AtomicReference<String>()
        webView.post {
            webView.evaluateJavascript(expression) { value ->
                result.set(value)
                latch.countDown()
            }
        }
        check(latch.await(EVALUATION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            "JavaScript evaluation timed out"
        }
        val raw = requireNotNull(result.get()) { "JavaScript returned no result" }
        return JSONArray("[$raw]").getString(0)
    }

    private fun findWebView(view: View): WebView? {
        if (view is WebView) return view
        if (view !is ViewGroup) return null
        for (index in 0 until view.childCount) {
            findWebView(view.getChildAt(index))?.let { return it }
        }
        return null
    }

    private companion object {
        const val AUDIO_TIMEOUT_MS = 10_000L
        const val BACKGROUND_SETTLE_MS = 1_000L
        const val EVALUATION_TIMEOUT_MS = 5_000L
        const val WEB_TIMEOUT_MS = 15_000L
        const val POLL_INTERVAL_MS = 100L
        const val MAXIMUM_FAILURE_CHARS = 16_384
        const val NOTIFICATION_ID = 0x4D4F4C
    }
}

private data class BackgroundEvidence(
    val backgroundCallbacks: Long,
    val lockedCallbacks: Long,
)
