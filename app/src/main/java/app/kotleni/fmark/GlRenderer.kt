package app.kotleni.fmark

import android.app.Activity
import android.view.Surface

object GlRenderer {
    init {
        System.loadLibrary("fmark")
    }

    external fun init(activity: Activity)

    external fun surfaceCreated(surface: Surface)
    external fun surfaceChanged(width: Int, height: Int)
    external fun surfaceDestroyed()
    external fun setResolution(w: Int, h: Int)
    external fun setPresentMode(mode: Int)
    external fun setAntialiasing(samples: Int)
    external fun setFurLength(value: Float)
    external fun setFurPopulation(population: Int)
    external fun stop()
    external fun getFps(): Float
    external fun getFpsHistory(max: Int): FloatArray
}