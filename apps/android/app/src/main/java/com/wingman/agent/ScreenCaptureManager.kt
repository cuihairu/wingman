package com.wingman.agent

import android.content.Context
import android.content.Intent
import android.graphics.PixelFormat
import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.media.ImageReader
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Handler
import android.os.HandlerThread
import android.util.Log

/**
 * 投屏采集管理器（A2，docs/android-agent-design.md §5.6）。
 *
 * MediaProjection + VirtualDisplay + ImageReader（RGBA_8888）：
 *  - 帧走推送：onImageAvailable（handler 线程）acquireLatestImage 取最新、
 *    拷平面字节、立即 close，再经 JNI 推给 C++ 缓存（Image 生命周期不出
 *    本类，规避跨线程持有）；
 *  - projection 停止（用户停止/系统回收）时自动释放并保持幂等。
 *
 * 时序约束（API 34）：调用方必须已以 mediaProjection 类型 startForeground
 * （WingmanService ACTION_START_CAPTURE 分支保证），否则 getMediaProjection
 * 抛 SecurityException。
 */
class ScreenCaptureManager(private val context: Context) {

    companion object {
        private const val TAG = "ScreenCapture"
        private const val VIRTUAL_DISPLAY_NAME = "wingman-capture"
    }

    private var projection: MediaProjection? = null
    private var virtualDisplay: VirtualDisplay? = null
    private var imageReader: ImageReader? = null
    private var readerThread: HandlerThread? = null
    private val stopLock = Any()

    /** 投屏是否已就绪（授权 + VirtualDisplay 建立）。 */
    var isActive: Boolean = false
        private set

    /** 用户授权回调结果启动投屏。失败（拒绝/异常）返回 false。 */
    fun start(resultCode: Int, data: Intent): Boolean {
        synchronized(stopLock) {
            if (isActive) {
                return true
            }
            val manager = context.getSystemService(
                Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
            val projection = try {
                manager.getMediaProjection(resultCode, data)
            } catch (e: Exception) {
                Log.e(TAG, "getMediaProjection failed", e)
                return false
            } ?: return false
            this.projection = projection

            val metrics = context.resources.displayMetrics
            val width = metrics.widthPixels
            val height = metrics.heightPixels
            val dpi = metrics.densityDpi

            readerThread = HandlerThread("wingman-capture").apply { start() }
            val handler = Handler(readerThread!!.looper)

            val reader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, 2)
            reader.setOnImageAvailableListener({ r ->
                // 只关心最新帧：acquireLatestImage 丢弃积压旧帧
                val image = r.acquireLatestImage() ?: return@setOnImageAvailableListener
                try {
                    val plane = image.planes[0]
                    val rowStride = plane.rowStride
                    val bytes = ByteArray(rowStride * image.height)
                    plane.buffer.get(bytes)
                    WingmanJni.nativeOnFrame(bytes, image.width, image.height, rowStride)
                } catch (e: Exception) {
                    Log.w(TAG, "frame copy failed", e)
                } finally {
                    image.close()
                }
            }, handler)

            projection.registerCallback(object : MediaProjection.Callback() {
                override fun onStop() {
                    // 用户/系统侧停止：释放并保持状态一致（不回调 projection.stop）
                    releaseInternal(callProjectionStop = false)
                }
            }, handler)

            virtualDisplay = projection.createVirtualDisplay(
                VIRTUAL_DISPLAY_NAME, width, height, dpi,
                DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                reader.surface, null, handler)
            imageReader = reader
            isActive = true
            Log.i(TAG, "capture started ${width}x${height}")
            return true
        }
    }

    /** 停止并释放全部资源（幂等；WingmanService 停止/销毁时调用）。 */
    fun stop() {
        synchronized(stopLock) {
            releaseInternal(callProjectionStop = true)
        }
    }

    private fun releaseInternal(callProjectionStop: Boolean) {
        if (!isActive && virtualDisplay == null && projection == null) {
            return
        }
        isActive = false
        try {
            virtualDisplay?.release()
        } catch (e: Exception) {
            Log.w(TAG, "virtual display release", e)
        }
        virtualDisplay = null
        try {
            imageReader?.close()
        } catch (e: Exception) {
            Log.w(TAG, "image reader close", e)
        }
        imageReader = null
        try {
            if (callProjectionStop) {
                projection?.stop()
            } else {
                // projection 已停：仅需解除回调引用
            }
        } finally {
            projection = null
            readerThread?.quitSafely()
            readerThread = null
        }
        Log.i(TAG, "capture released")
    }
}
