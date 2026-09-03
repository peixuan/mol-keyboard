// SPDX-License-Identifier: Apache-2.0
package cn.zhangpeixuan.molkeyboard

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioAttributes
import android.media.AudioDeviceCallback
import android.media.AudioDeviceInfo
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.Binder
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import cn.zhangpeixuan.molkeyboard.audio.AudioRuntime
import cn.zhangpeixuan.molkeyboard.audio.AudioStatus
import java.util.ArrayDeque
import java.util.LinkedHashMap

class AudioForegroundService : Service(), AudioManager.OnAudioFocusChangeListener {
    inner class LocalBinder : Binder() {
        val service: AudioForegroundService
            get() = this@AudioForegroundService
    }

    private val binder = LocalBinder()
    private val mainHandler = Handler(Looper.getMainLooper())
    private lateinit var audioManager: AudioManager
    private lateinit var focusRequest: AudioFocusRequest
    private var runtime: AudioRuntime? = null
    private var userStarted = false
    private var resumeAfterFocus = false
    private var uiForeground = true
    private var transportRunning = false
    private var playbackRunning = false
    private var metronomeRunning = false
    private var routeRevision = 0L
    private var lastStartResult = 0
    private var loadedSequence: ByteArray? = null
    private val pendingEvents = ArrayDeque<Long>()
    private val replayControls = LinkedHashMap<Long, ControlCommand>()

    private val routeCallback = object : AudioDeviceCallback() {
        override fun onAudioDevicesAdded(addedDevices: Array<out AudioDeviceInfo>) = scheduleRouteRestart()
        override fun onAudioDevicesRemoved(removedDevices: Array<out AudioDeviceInfo>) = scheduleRouteRestart()
    }

    private val becomingNoisyReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == AudioManager.ACTION_AUDIO_BECOMING_NOISY) pauseForRouteChange()
        }
    }

    private val disconnectWatchdog = object : Runnable {
        override fun run() {
            checkRuntime()
        }
    }

    override fun onCreate() {
        super.onCreate()
        audioManager = getSystemService(AudioManager::class.java)
        focusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
            )
            .setAcceptsDelayedFocusGain(false)
            .setOnAudioFocusChangeListener(this, mainHandler)
            .build()
        audioManager.registerAudioDeviceCallback(routeCallback, mainHandler)
        registerReceiver(becomingNoisyReceiver, IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY))
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                stopUserAudio()
                stopSelf()
            }
            ACTION_START -> startUserAudio()
        }
        return if (userStarted) START_STICKY else START_NOT_STICKY
    }

    @Synchronized
    fun startUserAudio(): Boolean {
        if (userStarted && runtime?.status()?.active == true) return true
        ensureNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification())
        val focus = audioManager.requestAudioFocus(focusRequest)
        if (focus != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            stopForeground(STOP_FOREGROUND_REMOVE)
            lastStartResult = focus
            return false
        }
        val candidate = runtime ?: AudioRuntime().also { runtime = it }
        lastStartResult = candidate.start()
        userStarted = lastStartResult == 0
        resumeAfterFocus = false
        if (userStarted) {
            restoreRuntimeState(candidate)
            mainHandler.removeCallbacks(disconnectWatchdog)
            mainHandler.postDelayed(disconnectWatchdog, WATCHDOG_INTERVAL_MS)
        } else {
            candidate.close()
            runtime = null
            audioManager.abandonAudioFocusRequest(focusRequest)
            stopForeground(STOP_FOREGROUND_REMOVE)
        }
        return userStarted
    }

    @Synchronized
    fun stopUserAudio() {
        mainHandler.removeCallbacks(disconnectWatchdog)
        runtime?.submitControl(NativeCommands.ALL_SOUND_OFF)
        runtime?.close()
        runtime = null
        userStarted = false
        resumeAfterFocus = false
        transportRunning = false
        playbackRunning = false
        metronomeRunning = false
        pendingEvents.clear()
        audioManager.abandonAudioFocusRequest(focusRequest)
        stopForeground(STOP_FOREGROUND_REMOVE)
    }

    @Synchronized
    fun submitControl(
        commandType: Int,
        gestureId: Long,
        integer0: Int,
        integer1: Int,
        integer2: Int,
        integer3: Int,
        scalar0: Float,
        scalar1: Float,
    ): Int {
        val current = runtime ?: return ERROR_INVALID_STATE
        val result = current.submitControl(
            commandType,
            gestureId,
            integer0,
            integer1,
            integer2,
            integer3,
            scalar0,
            scalar1,
        )
        if (result == 0) {
            rememberControl(
                commandType,
                gestureId,
                integer0,
                integer1,
                integer2,
                integer3,
                scalar0,
                scalar1,
            )
            when (commandType) {
                NativeCommands.TRANSPORT_START -> transportRunning = true
                NativeCommands.TRANSPORT_STOP -> transportRunning = false
                NativeCommands.PLAYBACK_START -> {
                    current.exportRecording()?.let { loadedSequence = it }
                    playbackRunning = true
                }
                NativeCommands.PLAYBACK_STOP -> playbackRunning = false
                NativeCommands.SET_METRONOME -> metronomeRunning = integer0 != 0
                NativeCommands.RESET_ENGINE -> {
                    transportRunning = false
                    playbackRunning = false
                    metronomeRunning = false
                    loadedSequence = null
                    replayControls.clear()
                }
            }
        }
        return result
    }

    @Synchronized
    fun pollEvents(): LongArray {
        pumpNativeEvents()
        val events = LongArray(pendingEvents.size)
        var offset = 0
        while (pendingEvents.isNotEmpty()) events[offset++] = pendingEvents.removeFirst()
        return events
    }

    @Synchronized
    fun status(): ServiceAudioStatus {
        val status = runtime?.status() ?: AudioStatus(0, 0, 0, 0, 0, 0, 0, lastStartResult, false, false)
        return ServiceAudioStatus(status, routeRevision, userStarted)
    }

    @Synchronized
    fun exportRecording(): ByteArray? =
        runtime?.exportRecording()?.also { loadedSequence = it } ?: loadedSequence?.copyOf()

    @Synchronized
    fun loadRecording(bytes: ByteArray): Int {
        if (bytes.isEmpty() || bytes.size > MAX_RECORDING_BYTES) return ERROR_INVALID_ARGUMENT
        val result = runtime?.loadRecording(bytes) ?: ERROR_INVALID_STATE
        if (result == 0) loadedSequence = bytes.copyOf()
        return result
    }

    @Synchronized
    fun onUiForegrounded() {
        uiForeground = true
    }

    @Synchronized
    fun onUiBackgrounded() {
        uiForeground = false
        runtime?.submitControl(NativeCommands.ALL_NOTES_OFF)
        if (!allowsBackgroundContinuation()) stopUserAudio()
    }

    override fun onAudioFocusChange(focusChange: Int) {
        when (focusChange) {
            AudioManager.AUDIOFOCUS_GAIN -> {
                if (resumeAfterFocus) {
                    resumeAfterFocus = false
                    startUserAudio()
                }
            }
            AudioManager.AUDIOFOCUS_LOSS -> {
                resumeAfterFocus = false
                stopUserAudio()
                stopSelf()
            }
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT,
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK,
            -> {
                resumeAfterFocus = userStarted && (uiForeground || allowsBackgroundContinuation())
                stopRuntimeForInterruption()
            }
        }
    }

    override fun onTaskRemoved(rootIntent: Intent?) {
        if (!allowsBackgroundContinuation()) {
            stopUserAudio()
            stopSelf()
        }
        super.onTaskRemoved(rootIntent)
    }

    override fun onDestroy() {
        mainHandler.removeCallbacksAndMessages(null)
        audioManager.unregisterAudioDeviceCallback(routeCallback)
        unregisterReceiver(becomingNoisyReceiver)
        stopUserAudio()
        super.onDestroy()
    }

    @Synchronized
    private fun stopRuntimeForInterruption() {
        runtime?.submitControl(NativeCommands.ALL_SOUND_OFF)
        runtime?.close()
        runtime = null
        userStarted = false
        mainHandler.removeCallbacks(disconnectWatchdog)
    }

    @Synchronized
    private fun restartForRoute() {
        if (!userStarted) return
        val current = runtime ?: return
        current.submitControl(NativeCommands.ALL_SOUND_OFF)
        current.stop()
        lastStartResult = current.start()
        routeRevision += 1L
        if (lastStartResult == 0) restoreRuntimeState(current) else stopUserAudio()
    }

    private fun scheduleRouteRestart() {
        mainHandler.removeCallbacks(routeRestart)
        mainHandler.postDelayed(routeRestart, ROUTE_DEBOUNCE_MS)
    }

    private val routeRestart = Runnable { restartForRoute() }

    private fun pauseForRouteChange() {
        resumeAfterFocus = false
        stopUserAudio()
    }

    private fun allowsBackgroundContinuation(): Boolean =
        playbackRunning || metronomeRunning

    @Synchronized
    private fun checkRuntime() {
        pumpNativeEvents()
        val status = runtime?.status()
        if (userStarted && status?.disconnected == true) restartForRoute()
        if (userStarted && !uiForeground && !allowsBackgroundContinuation()) {
            stopUserAudio()
            stopSelf()
            return
        }
        if (userStarted) mainHandler.postDelayed(disconnectWatchdog, WATCHDOG_INTERVAL_MS)
    }

    private fun pumpNativeEvents() {
        val events = runtime?.pollEvents() ?: return
        var offset = 0
        while (offset + EVENT_FIELD_COUNT <= events.size) {
            when (events[offset].toInt()) {
                EVENT_RECORDING_CHANGED -> {
                    if (events[offset + 4] == 0L) {
                        runtime?.exportRecording()?.let { loadedSequence = it }
                    }
                }
                EVENT_PLAYBACK_CHANGED -> playbackRunning = events[offset + 4] != 0L
            }
            while (pendingEvents.size + EVENT_FIELD_COUNT > MAX_PENDING_EVENT_FIELDS) {
                repeat(EVENT_FIELD_COUNT) { pendingEvents.removeFirst() }
            }
            repeat(EVENT_FIELD_COUNT) { index -> pendingEvents.addLast(events[offset + index]) }
            offset += EVENT_FIELD_COUNT
        }
    }

    private fun rememberControl(
        commandType: Int,
        gestureId: Long,
        integer0: Int,
        integer1: Int,
        integer2: Int,
        integer3: Int,
        scalar0: Float,
        scalar1: Float,
    ) {
        val persistent = commandType in PERSISTENT_CONTROLS
        if (!persistent) return
        val key = (commandType.toLong() shl 32) or
            (if (commandType == NativeCommands.SET_PARAMETER) integer0.toLong() and 0xFFFF_FFFFL else 0L)
        replayControls[key] = ControlCommand(
            commandType,
            gestureId,
            integer0,
            integer1,
            integer2,
            integer3,
            scalar0,
            scalar1,
        )
    }

    private fun restoreRuntimeState(current: AudioRuntime) {
        loadedSequence?.let { current.loadRecording(it) }
        replayControls.values.forEach { command ->
            current.submitControl(
                command.type,
                command.gestureId,
                command.integer0,
                command.integer1,
                command.integer2,
                command.integer3,
                command.scalar0,
                command.scalar1,
            )
        }
        if (playbackRunning) current.submitControl(NativeCommands.PLAYBACK_START)
        if (transportRunning) current.submitControl(NativeCommands.TRANSPORT_START)
    }

    private fun ensureNotificationChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                NOTIFICATION_CHANNEL,
                getString(R.string.audio_channel_name),
                NotificationManager.IMPORTANCE_LOW,
            ).apply {
                description = getString(R.string.audio_channel_description)
                setSound(null, null)
            },
        )
    }

    private fun buildNotification(): Notification {
        val openIntent = PendingIntent.getActivity(
            this,
            1,
            Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val stopIntent = PendingIntent.getService(
            this,
            2,
            Intent(this, AudioForegroundService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        return Notification.Builder(this, NOTIFICATION_CHANNEL)
            .setSmallIcon(R.drawable.ic_mol_keyboard)
            .setContentTitle(getString(R.string.audio_notification_title))
            .setContentText(getString(R.string.audio_notification_text))
            .setContentIntent(openIntent)
            .setOngoing(true)
            .setCategory(Notification.CATEGORY_SERVICE)
            .addAction(Notification.Action.Builder(null, getString(R.string.stop_audio), stopIntent).build())
            .build()
    }

    companion object {
        val ACTION_START = "${BuildConfig.APPLICATION_ID}.action.START_AUDIO"
        val ACTION_STOP = "${BuildConfig.APPLICATION_ID}.action.STOP_AUDIO"
        private const val NOTIFICATION_CHANNEL = "mol_keyboard_audio"
        private const val NOTIFICATION_ID = 0x4D4F4C
        private const val ERROR_INVALID_STATE = 2
        private const val ERROR_INVALID_ARGUMENT = 1
        private const val EVENT_FIELD_COUNT = 5
        private const val EVENT_RECORDING_CHANGED = 7
        private const val EVENT_PLAYBACK_CHANGED = 14
        private const val MAX_PENDING_EVENT_FIELDS = 256 * EVENT_FIELD_COUNT
        private const val MAX_RECORDING_BYTES = 2 * 1024 * 1024
        private const val ROUTE_DEBOUNCE_MS = 250L
        private const val WATCHDOG_INTERVAL_MS = 500L
        private val PERSISTENT_CONTROLS = setOf(
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
            NativeCommands.SET_METRONOME,
            NativeCommands.SET_PORTAMENTO,
        )
    }
}

private data class ControlCommand(
    val type: Int,
    val gestureId: Long,
    val integer0: Int,
    val integer1: Int,
    val integer2: Int,
    val integer3: Int,
    val scalar0: Float,
    val scalar1: Float,
)

data class ServiceAudioStatus(
    val audio: AudioStatus,
    val routeRevision: Long,
    val userStarted: Boolean,
)
