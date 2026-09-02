// SPDX-License-Identifier: Apache-2.0
package cn.zhangpeixuan.molkeyboard

import android.util.Base64
import android.webkit.JavascriptInterface
import org.json.JSONArray
import org.json.JSONObject

internal class NativeWebBridge(
    private val activity: MainActivity,
    private val service: () -> AudioForegroundService?,
) {
    @JavascriptInterface
    fun dispatch(requestText: String): String = try {
        require(requestText.length in 2..MAX_REQUEST_CHARS) { "Request size is invalid" }
        val request = JSONObject(requestText)
        requireExactKeys(request, setOf("version", "method", "params"))
        require(request.getInt("version") == BRIDGE_VERSION) { "Unsupported bridge version" }
        val method = request.getString("method")
        require(method.length in 1..64) { "Method size is invalid" }
        val params = request.getJSONObject("params")
        dispatchMethod(method, params)
    } catch (error: Exception) {
        failure(error.message ?: "Native bridge request failed")
    }

    private fun dispatchMethod(method: String, params: JSONObject): String = when (method) {
        "runtime.start" -> {
            requireExactKeys(params, emptySet())
            activity.startNativeAudio()
            success()
        }
        "runtime.stop" -> {
            requireExactKeys(params, emptySet())
            activity.stopNativeAudio()
            success()
        }
        "runtime.status" -> {
            requireExactKeys(params, emptySet())
            val state = requireService().status()
            JSONObject()
                .put("ok", true)
                .put("active", state.audio.active)
                .put("userStarted", state.userStarted)
                .put("sampleRate", state.audio.sampleRate)
                .put("framesPerBurst", state.audio.framesPerBurst)
                .put("audioApi", state.audio.audioApi)
                .put("callbackCount", state.audio.callbackCount)
                .put("renderedFrames", state.audio.renderedFrames)
                .put("renderFailures", state.audio.renderFailures)
                .put("nonFiniteSamples", state.audio.nonFiniteSamples)
                .put("lastError", state.audio.lastError)
                .put("disconnected", state.audio.disconnected)
                .put("routeRevision", state.routeRevision)
                .toString()
        }
        "command.submit" -> submitCommand(params)
        "events.poll" -> {
            requireExactKeys(params, emptySet())
            val values = requireService().pollEvents()
            require(values.size <= MAX_EVENT_FIELDS && values.size % EVENT_FIELD_COUNT == 0) {
                "Native event batch is invalid"
            }
            val events = JSONArray()
            values.forEach(events::put)
            JSONObject().put("ok", true).put("events", events).toString()
        }
        "recording.export" -> {
            requireExactKeys(params, emptySet())
            val bytes = requireNotNull(requireService().exportRecording()) {
                "No complete recording is available"
            }
            require(bytes.size in 1..MAX_RECORDING_BYTES) { "Recording size is invalid" }
            JSONObject()
                .put("ok", true)
                .put("base64", Base64.encodeToString(bytes, Base64.NO_WRAP))
                .toString()
        }
        "recording.load" -> {
            requireExactKeys(params, setOf("base64"))
            val encoded = params.getString("base64")
            require(encoded.length in 1..MAX_BASE64_CHARS) { "Recording payload is invalid" }
            val bytes = Base64.decode(encoded, Base64.NO_WRAP)
            require(bytes.size in 1..MAX_RECORDING_BYTES) { "Recording size is invalid" }
            val result = requireService().loadRecording(bytes)
            JSONObject().put("ok", result == 0).put("result", result).toString()
        }
        else -> throw IllegalArgumentException("Method is not allowed")
    }

    private fun submitCommand(params: JSONObject): String {
        requireExactKeys(params, setOf("type", "gesture", "i0", "i1", "i2", "i3", "f0", "f1"))
        val commandType = params.getInt("type")
        require(commandType in ALLOWED_COMMANDS) { "Command is not allowed" }
        val gesture = params.getLong("gesture")
        require(gesture >= 0L) { "Gesture is invalid" }
        val scalar0 = params.getDouble("f0")
        val scalar1 = params.getDouble("f1")
        require(scalar0.isFinite() && scalar1.isFinite()) { "Scalar is not finite" }
        val result = requireService().submitControl(
            commandType,
            gesture,
            params.getInt("i0"),
            params.getInt("i1"),
            params.getInt("i2"),
            params.getInt("i3"),
            scalar0.toFloat(),
            scalar1.toFloat(),
        )
        return JSONObject().put("ok", result == 0).put("result", result).toString()
    }

    private fun requireService(): AudioForegroundService =
        service() ?: throw IllegalStateException("Native audio service is not ready")

    private fun requireExactKeys(value: JSONObject, expected: Set<String>) {
        val actual = mutableSetOf<String>()
        value.keys().forEachRemaining(actual::add)
        require(actual == expected) { "Unexpected or missing fields" }
    }

    private fun success(): String = JSONObject().put("ok", true).toString()

    private fun failure(message: String): String = JSONObject()
        .put("ok", false)
        .put("error", message.take(MAX_ERROR_CHARS))
        .toString()

    private companion object {
        const val BRIDGE_VERSION = 1
        const val MAX_REQUEST_CHARS = 2_800_000
        const val MAX_RECORDING_BYTES = 2 * 1024 * 1024
        const val MAX_BASE64_CHARS = 2_796_204
        const val MAX_ERROR_CHARS = 256
        const val MAX_EVENT_FIELDS = 64 * 5
        const val EVENT_FIELD_COUNT = 5
        val ALLOWED_COMMANDS = setOf(
            NativeCommands.NOTE_ON,
            NativeCommands.NOTE_OFF,
            NativeCommands.SUSTAIN,
            NativeCommands.ALL_NOTES_OFF,
            NativeCommands.ALL_SOUND_OFF,
            NativeCommands.SET_MASTER_GAIN,
            NativeCommands.SET_PRESET,
            NativeCommands.SET_PARAMETER,
            NativeCommands.SET_OCTAVE,
            NativeCommands.SET_TRANSPOSE,
            NativeCommands.SET_SCALE,
            NativeCommands.SET_CHORD,
            NativeCommands.SET_ARPEGGIATOR,
            NativeCommands.SET_TEMPO,
            NativeCommands.SET_TIME_SIGNATURE,
            NativeCommands.TRANSPORT_START,
            NativeCommands.TRANSPORT_STOP,
            NativeCommands.RECORD_START,
            NativeCommands.RECORD_STOP,
            NativeCommands.PLAYBACK_START,
            NativeCommands.PLAYBACK_STOP,
            NativeCommands.RESET_ENGINE,
            NativeCommands.SET_METRONOME,
            NativeCommands.SET_PORTAMENTO,
        )
    }
}
