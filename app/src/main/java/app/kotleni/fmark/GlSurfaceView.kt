package app.kotleni.fmark

import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView

class GlSurfaceView(context: Context) : SurfaceView(context), SurfaceHolder.Callback {

    init {
        holder.addCallback(this)
    }

    fun setRenderResolution(width: Int, height: Int) {
        if (width > 0 && height > 0) {
            holder.setFixedSize(width, height)
        } else {
            holder.setSizeFromLayout()
        }
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        GlRenderer.surfaceCreated(holder.surface)
        if (width > 0 && height > 0) GlRenderer.surfaceChanged(width, height)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        GlRenderer.surfaceChanged(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        GlRenderer.surfaceDestroyed()
    }
}