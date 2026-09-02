// SPDX-License-Identifier: Apache-2.0
package cn.zhangpeixuan.molkeyboard.audio

import java.io.Closeable

data class AudioStatus(
    val sampleRate: Int,
    val framesPerBurst: Int,
    val audioApi: Int,
    val callbackCount: Long,
    val renderedFrames: Long,
    val renderFailures: Int,
    val nonFiniteSamples: Int,
    val lastError: Int,
    val active: Boolean,
    val disconnected: Boolean,
)

class AudioRuntime : Closeable {
    private var handle: Long = NativeAudio.nativeCreate()

    init {
        check(handle != 0L) { "Unable to allocate native audio runtime" }
    }

    @Synchronized
    fun start(): Int = requireHandle().let(NativeAudio::nativeStart)

    @Synchronized
    fun stop() {
        if (handle != 0L) NativeAudio.nativeStop(handle)
    }

    @Synchronized
    fun noteOn(note: Int, velocity: Float, gestureId: Long): Int =
        NativeAudio.nativeNoteOn(requireHandle(), note, velocity, gestureId)

    @Synchronized
    fun noteOff(note: Int, gestureId: Long): Int =
        NativeAudio.nativeNoteOff(requireHandle(), note, gestureId)

    @Synchronized
    fun submitControl(
        commandType: Int,
        gestureId: Long = 0,
        integer0: Int = 0,
        integer1: Int = 0,
        integer2: Int = 0,
        integer3: Int = 0,
        scalar0: Float = 0f,
        scalar1: Float = 0f,
    ): Int = NativeAudio.nativeSubmitControl(
        requireHandle(),
        commandType,
        gestureId,
        integer0,
        integer1,
        integer2,
        integer3,
        scalar0,
        scalar1,
    )

    @Synchronized
    fun pollEvents(): LongArray = NativeAudio.nativePollEvents(requireHandle())

    @Synchronized
    fun exportRecording(): ByteArray? = NativeAudio.nativeExportRecording(requireHandle())

    @Synchronized
    fun loadRecording(bytes: ByteArray): Int =
        NativeAudio.nativeLoadRecording(requireHandle(), bytes)

    @Synchronized
    fun status(): AudioStatus {
        val values = NativeAudio.nativeStatus(requireHandle())
        check(values.size == STATUS_FIELD_COUNT) { "Invalid native audio status" }
        return AudioStatus(
            sampleRate = values[0].toInt(),
            framesPerBurst = values[1].toInt(),
            audioApi = values[2].toInt(),
            callbackCount = values[3],
            renderedFrames = values[4],
            renderFailures = values[5].toInt(),
            nonFiniteSamples = values[6].toInt(),
            lastError = values[7].toInt(),
            active = values[8] != 0L,
            disconnected = values[9] != 0L,
        )
    }

    @Synchronized
    override fun close() {
        if (handle != 0L) {
            NativeAudio.nativeStop(handle)
            NativeAudio.nativeDestroy(handle)
            handle = 0L
        }
    }

    private fun requireHandle(): Long {
        check(handle != 0L) { "Audio runtime is closed" }
        return handle
    }

    private companion object {
        const val STATUS_FIELD_COUNT = 10
    }
}

internal object NativeAudio {
    init {
        System.loadLibrary("mol_android_audio")
    }

    external fun nativeCreate(): Long
    external fun nativeStart(handle: Long): Int
    external fun nativeStop(handle: Long)
    external fun nativeDestroy(handle: Long)
    external fun nativeNoteOn(handle: Long, note: Int, velocity: Float, gestureId: Long): Int
    external fun nativeNoteOff(handle: Long, note: Int, gestureId: Long): Int
    external fun nativeStatus(handle: Long): LongArray
    external fun nativeSubmitControl(
        handle: Long,
        commandType: Int,
        gestureId: Long,
        integer0: Int,
        integer1: Int,
        integer2: Int,
        integer3: Int,
        scalar0: Float,
        scalar1: Float,
    ): Int
    external fun nativePollEvents(handle: Long): LongArray
    external fun nativeExportRecording(handle: Long): ByteArray?
    external fun nativeLoadRecording(handle: Long, bytes: ByteArray): Int
}
