package app.kotleni.fmark.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.kotleni.fmark.Antialiasing
import app.kotleni.fmark.FmarkSettings
import app.kotleni.fmark.PresentMode
import app.kotleni.fmark.ResolutionOption
import app.kotleni.fmark.toSpacedString
import kotlin.math.roundToInt

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    settings: FmarkSettings,
    onSettingsChange: (FmarkSettings) -> Unit,
    onStartBenchmark: () -> Unit,
) {
    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .safeDrawingPadding()
                .verticalScroll(rememberScrollState())
                .padding(24.dp),
        ) {
            Text(
                text = "fmark",
                style = MaterialTheme.typography.headlineMedium,
                fontWeight = FontWeight.Bold,
            )
            Text(
                text = "Android vulkan benchmark",
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(24.dp))

            // ---- Resolution ----
            SettingsDropdown(
                label = "Resolution",
                value = settings.resolution.label,
                options = ResolutionOption.entries.map { it.label },
                onSelect = { idx ->
                    onSettingsChange(settings.copy(resolution = ResolutionOption.entries[idx]))
                },
            )
            Text(
                text = "The selected aspect ratio is preserved.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(16.dp))

            // ---- Present mode ----
            SettingsDropdown(
                label = "Frame pacing",
                value = settings.presentMode.label,
                options = PresentMode.entries.map { it.label },
                onSelect = { idx ->
                    onSettingsChange(settings.copy(presentMode = PresentMode.entries[idx]))
                },
            )
            Spacer(Modifier.height(16.dp))

            SettingsDropdown(
                label = "Antialiasing",
                value = settings.antialiasing.label,
                options = Antialiasing.entries.map { it.label },
                onSelect = { idx ->
                    onSettingsChange(settings.copy(antialiasing = Antialiasing.entries[idx]))
                },
            )
            Spacer(Modifier.height(16.dp))

            // ---- Fur length ----
            Text(
                text = "Fur length  ${"%.2f".format(settings.furLength)}",
                style = MaterialTheme.typography.bodyLarge,
            )
            Slider(
                value = settings.furLength,
                onValueChange = { onSettingsChange(settings.copy(furLength = it)) },
                valueRange = 0.10f..1.00f,
            )
            Spacer(Modifier.height(8.dp))

            // ---- Fur population ----
            Text(
                text = "Fur population  ${settings.furPopulation.toSpacedString()} hairs",
                style = MaterialTheme.typography.bodyLarge,
            )
            Slider(
                value = settings.furPopulation.toFloat(),
                onValueChange = {
                    val population = (it / FmarkSettings.FUR_POPULATION_STEP)
                        .roundToInt() * FmarkSettings.FUR_POPULATION_STEP
                    onSettingsChange(settings.copy(furPopulation = population))
                },
                valueRange = FmarkSettings.MIN_FUR_POPULATION.toFloat()..
                    FmarkSettings.MAX_FUR_POPULATION.toFloat(),
                steps = (FmarkSettings.MAX_FUR_POPULATION - FmarkSettings.MIN_FUR_POPULATION) /
                    FmarkSettings.FUR_POPULATION_STEP - 1,
            )
            Spacer(Modifier.height(8.dp))

            // ---- Overlays ----
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = "Show FPS overlays",
                    style = MaterialTheme.typography.bodyLarge,
                    modifier = Modifier.weight(1f),
                )
                Switch(
                    checked = settings.showOverlays,
                    onCheckedChange = { onSettingsChange(settings.copy(showOverlays = it)) },
                )
            }

            Spacer(Modifier.height(32.dp))
            Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                Button(
                    onClick = onStartBenchmark,
                    modifier = Modifier.fillMaxWidth(0.8f),
                ) {
                    Text("Start benchmark", fontSize = 18.sp)
                }
            }
            Spacer(Modifier.height(40.dp))
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SettingsDropdown(
    label: String,
    value: String,
    options: List<String>,
    onSelect: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }

    Text(label, style = MaterialTheme.typography.bodyLarge)
    Spacer(Modifier.height(8.dp))
    ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = { expanded = it }) {
        OutlinedTextField(
            value = value,
            onValueChange = {},
            readOnly = true,
            label = { Text(label) },
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            modifier = Modifier
                .menuAnchor()
                .fillMaxWidth(),
        )
        ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            options.forEachIndexed { index, option ->
                DropdownMenuItem(
                    text = { Text(option) },
                    onClick = {
                        onSelect(index)
                        expanded = false
                    },
                )
            }
        }
    }
}