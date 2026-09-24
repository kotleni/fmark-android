package app.kotleni.fmark

import android.content.Context
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import app.kotleni.fmark.ui.BenchmarkScreen
import app.kotleni.fmark.ui.MainScreen
import app.kotleni.fmark.ui.theme.FmarkTheme
import kotlin.math.roundToInt

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        AppSettingsStore.load(this)
        setContent {
            FmarkTheme {
                FmarkApp()
            }
        }
    }
}

@Composable
fun FmarkApp() {
    var screen by rememberSaveable { mutableStateOf(Screen.Main) }
    val context = LocalContext.current
    val settings = AppSettingsStore.settings

    when (screen) {
        Screen.Main -> MainScreen(
            settings = settings,
            onSettingsChange = { AppSettingsStore.save(context, it) },
            onStartBenchmark = {
                AppSettingsStore.load(context)
                screen = Screen.Benchmark
            },
        )
        Screen.Benchmark -> BenchmarkScreen(
            settings = settings,
            onStop = { screen = Screen.Main },
        )
    }
}

object AppSettingsStore {
    private const val PREFS = "fmark_settings"
    var settings by mutableStateOf(FmarkSettings())
    private var loaded = false

    fun load(context: Context) {
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val legacyLayers = prefs.getInt("furLayers", 50).coerceIn(10, 80)
        val legacyProgress = (legacyLayers - 10).toFloat() / 70.0f
        val migratedPopulation = (
            (FmarkSettings.MIN_FUR_POPULATION +
                legacyProgress * (FmarkSettings.MAX_FUR_POPULATION - FmarkSettings.MIN_FUR_POPULATION)) /
                FmarkSettings.FUR_POPULATION_STEP
            ).roundToInt() * FmarkSettings.FUR_POPULATION_STEP
        settings = FmarkSettings(
            resolution = ResolutionOption.entries.getOrElse(prefs.getInt("resolution", 0)) {
                ResolutionOption.entries.first()
            },
            presentMode = PresentMode.entries.getOrElse(prefs.getInt("presentMode", 0)) {
                PresentMode.UNCAPPED
            },
            antialiasing = Antialiasing.entries.getOrElse(prefs.getInt("antialiasing", 0)) {
                Antialiasing.OFF
            },
            furLength = prefs.getFloat("furLength", FmarkSettings.DEFAULT_FUR_LENGTH),
            furPopulation = prefs.getInt("furPopulation", migratedPopulation),
            showOverlays = prefs.getBoolean("showOverlays", true),
        )
        loaded = true
    }

    fun save(context: Context, s: FmarkSettings) {
        settings = s
        loaded = true
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        prefs.edit()
            .putInt("resolution", s.resolution.ordinal)
            .putInt("presentMode", s.presentMode.ordinal)
            .putInt("antialiasing", s.antialiasing.ordinal)
            .remove("geometryDetail")
            .putFloat("furLength", s.furLength)
            .putInt("furPopulation", s.furPopulation)
            .remove("furLayers")
            .putBoolean("showOverlays", s.showOverlays)
            .apply()
    }
}

enum class Screen { Main, Benchmark }