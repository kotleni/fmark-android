package app.kotleni.fmark.ui

import android.app.Activity
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import app.kotleni.fmark.FmarkSettings
import app.kotleni.fmark.GlRenderer
import app.kotleni.fmark.GlSurfaceView
import app.kotleni.fmark.toSpacedString
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive

@Composable
fun BenchmarkScreen(
    settings: FmarkSettings,
    onStop: () -> Unit,
) {
    var fps by remember { mutableFloatStateOf(0f) }
    var history by remember { mutableStateOf(FloatArray(0)) }
    var running by remember { mutableStateOf(true) }

    if (running) {
        LaunchedEffect(Unit) {
            while (isActive) {
                fps = GlRenderer.getFps()
                history = GlRenderer.getFpsHistory(300)
                delay(150)
            }
        }
    }

    val stopAction: () -> Unit = {
        if (running) {
            running = false
            GlRenderer.stop()
            onStop()
        }
    }

    BackHandler { stopAction() }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        if (maxHeight > maxWidth) {
            PortraitBenchmark(
                settings = settings,
                fps = fps,
                history = history,
                metricsReady = history.isNotEmpty(),
                stopAction = stopAction,
            )
        } else {
            LandscapeBenchmark(
                settings = settings,
                fps = fps,
                history = history,
                metricsReady = history.isNotEmpty(),
                stopAction = stopAction,
            )
        }
    }
}

@Composable
private fun PortraitBenchmark(
    settings: FmarkSettings,
    fps: Float,
    history: FloatArray,
    metricsReady: Boolean,
    stopAction: () -> Unit,
) {
    val renderAspect = if (settings.resolution.width > 0 && settings.resolution.height > 0) {
        settings.resolution.width.toFloat() / settings.resolution.height
    } else {
        4.0f / 3.0f
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
            .safeDrawingPadding(),
    ) {
        // Portrait mode keeps the render above all benchmark controls.
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .aspectRatio(renderAspect)
                .background(Color.Black),
        ) {
            BenchmarkSurface(settings, Modifier.fillMaxSize())
        }

        Column(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .padding(horizontal = 20.dp, vertical = 16.dp),
        ) {
            if (settings.showOverlays) {
                BenchmarkDetails(settings, fps, metricsReady)
                Spacer(Modifier.weight(1f))
                FpsGraph(
                    values = history,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(16.dp))
            } else {
                Spacer(Modifier.weight(1f))
            }

            StopButton(stopAction, Modifier.fillMaxWidth())
        }
    }
}

@Composable
private fun LandscapeBenchmark(
    settings: FmarkSettings,
    fps: Float,
    history: FloatArray,
    metricsReady: Boolean,
    stopAction: () -> Unit,
) {
    Box(Modifier.fillMaxSize().background(Color.Black)) {
        BenchmarkSurface(settings, Modifier.fillMaxSize())
        StopButton(stopAction, Modifier.align(Alignment.BottomEnd).padding(20.dp))

        if (settings.showOverlays) {
            RenderOverlay(
                settings = settings,
                fps = fps,
                metricsReady = metricsReady,
                modifier = Modifier.align(Alignment.TopStart).padding(16.dp),
            )
            FpsGraph(
                values = history,
                modifier = Modifier
                    .align(Alignment.BottomStart)
                    .padding(16.dp)
                    .width(300.dp),
                compact = true,
            )
        }
    }
}

@Composable
private fun BenchmarkSurface(settings: FmarkSettings, modifier: Modifier = Modifier) {
    val activity = LocalContext.current as Activity
    AndroidView(
        factory = { context ->
            val surfaceView = GlSurfaceView(context)
            surfaceView.setRenderResolution(settings.resolution.width, settings.resolution.height)
            GlRenderer.init(activity)
            GlRenderer.setResolution(settings.resolution.width, settings.resolution.height)
            GlRenderer.setPresentMode(settings.presentMode.ordinal)
            GlRenderer.setAntialiasing(settings.antialiasing.samples)
            GlRenderer.setFurLength(settings.furLength)
            GlRenderer.setFurPopulation(settings.furPopulation)
            surfaceView
        },
        modifier = modifier,
    )
}

@Composable
private fun RenderOverlay(
    settings: FmarkSettings,
    fps: Float,
    metricsReady: Boolean,
    modifier: Modifier = Modifier,
) {
    Column(modifier = modifier) {
        Text(
            text = if (metricsReady) "${fps.toInt()} fps" else "Preparing renderer…",
            color = Color(0xFF4AF626),
            fontSize = 36.sp,
            fontWeight = FontWeight.Bold,
        )
        Text(
            text = settings.resolution.label,
            color = Color.White.copy(alpha = 0.75f),
            fontSize = 14.sp,
        )
        Text(
            text = "fur ${settings.furPopulation.toSpacedString()} hairs · ${settings.presentMode.label}",
            color = Color.White.copy(alpha = 0.6f),
            fontSize = 12.sp,
        )
        Text(
            text = settings.antialiasing.label,
            color = Color.White.copy(alpha = 0.6f),
            fontSize = 12.sp,
        )
    }
}

@Composable
private fun BenchmarkDetails(settings: FmarkSettings, fps: Float, metricsReady: Boolean) {
    Text(
        text = if (metricsReady) "${fps.toInt()} fps" else "Preparing renderer…",
        color = MaterialTheme.colorScheme.primary,
        style = MaterialTheme.typography.displaySmall,
        fontWeight = FontWeight.Bold,
    )
    Text(
        text = settings.resolution.label,
        color = MaterialTheme.colorScheme.onSurface,
        style = MaterialTheme.typography.titleMedium,
    )
    Text(
        text = "fur ${settings.furPopulation.toSpacedString()} hairs · ${settings.presentMode.label}",
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        style = MaterialTheme.typography.bodyMedium,
    )
    Text(
        text = settings.antialiasing.label,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        style = MaterialTheme.typography.bodyMedium,
    )
}

@Composable
private fun StopButton(onClick: () -> Unit, modifier: Modifier = Modifier) {
    Button(
        onClick = onClick,
        colors = ButtonDefaults.buttonColors(
            containerColor = MaterialTheme.colorScheme.error,
            contentColor = MaterialTheme.colorScheme.onError,
        ),
        modifier = modifier,
    ) {
        Text("Stop", fontSize = 18.sp)
    }
}

@Composable
private fun FpsGraph(
    values: FloatArray,
    modifier: Modifier = Modifier,
    compact: Boolean = false,
) {
    val primary = MaterialTheme.colorScheme.primary
    val onSurface = MaterialTheme.colorScheme.onSurface
    val onSurfaceVariant = MaterialTheme.colorScheme.onSurfaceVariant
    val gridColor = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.55f)
    val currentFps = values.lastOrNull()

    Card(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainer,
        ),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.45f)),
    ) {
        Column(Modifier.fillMaxWidth().padding(14.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Column(Modifier.weight(1f)) {
                    Text(
                        text = "Frame rate",
                        color = onSurface,
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold,
                    )
                    Text(
                        text = "Rolling 5 minutes",
                        color = onSurfaceVariant,
                        style = MaterialTheme.typography.labelSmall,
                    )
                }
                Text(
                    text = if (currentFps == null) "—" else "${currentFps.toInt()} fps",
                    color = primary,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                )
            }

            Spacer(Modifier.height(10.dp))
            Canvas(
                Modifier
                    .fillMaxWidth()
                    .height(if (compact) 84.dp else 132.dp),
            ) {
                val lineBottom = size.height - 2.dp.toPx()
                for (i in 0..3) {
                    val y = size.height * i / 3.0f
                    drawLine(
                        color = gridColor,
                        start = Offset(0f, y),
                        end = Offset(size.width, y),
                        strokeWidth = 1.dp.toPx(),
                    )
                }

                if (values.isEmpty()) return@Canvas

                val peak = values.maxOrNull()?.coerceAtLeast(1f) ?: 1f
                val maxFps = maxOf(60f, peak * 1.15f)
                val stepX = if (values.size > 1) size.width / (values.size - 1) else 0f
                val linePath = Path()
                val fillPath = Path()
                var lastX = 0f
                var lastY = lineBottom

                values.forEachIndexed { index, value ->
                    val x = if (values.size > 1) index * stepX else size.width
                    val y = lineBottom - (value / maxFps).coerceIn(0f, 1f) * lineBottom
                    if (index == 0) {
                        linePath.moveTo(x, y)
                        fillPath.moveTo(x, lineBottom)
                        fillPath.lineTo(x, y)
                    } else {
                        linePath.lineTo(x, y)
                        fillPath.lineTo(x, y)
                    }
                    lastX = x
                    lastY = y
                }

                fillPath.lineTo(lastX, lineBottom)
                fillPath.close()
                drawPath(
                    path = fillPath,
                    brush = Brush.verticalGradient(
                        colors = listOf(primary.copy(alpha = 0.30f), primary.copy(alpha = 0.02f)),
                        startY = 0f,
                        endY = size.height,
                    ),
                )
                drawPath(
                    path = linePath,
                    brush = Brush.horizontalGradient(
                        colors = listOf(primary.copy(alpha = 0.55f), primary),
                    ),
                    style = Stroke(
                        width = 2.5.dp.toPx(),
                        cap = StrokeCap.Round,
                        join = StrokeJoin.Round,
                    ),
                )
                drawCircle(
                    color = primary,
                    radius = 3.dp.toPx(),
                    center = Offset(lastX, lastY),
                )
            }
        }
    }
}
