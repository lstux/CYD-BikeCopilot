package io.github.lstux.blecompanion

import android.bluetooth.BluetoothManager
import android.content.Intent
import android.location.LocationManager
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.launch

/**
 * L'écran. Une « Activity » est un écran de l'appli ; Android la crée, la met en pause, la détruit (rotation, retour…).
 * Elle ne fait donc AUCUN travail de fond : elle démarre/arrête le service et affiche son état (Bridge.state).
 * Son aspect est décrit dans res/layout/activity_main.xml.
 */
class MainActivity : AppCompatActivity() {
    private lateinit var toggle: Button
    private lateinit var messageText: TextView
    private lateinit var gpsText: TextView
    private lateinit var devicesText: TextView

    // Fenêtre de demande de permissions ; le résultat arrive dans le bloc ci-dessous.
    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
            if (Permissions.hasEssential(this)) startBridge() else say("Permissions refusées : Bluetooth et position sont nécessaires.")
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        toggle = findViewById(R.id.toggleButton)
        messageText = findViewById(R.id.messageText)
        gpsText = findViewById(R.id.gpsText)
        devicesText = findViewById(R.id.devicesText)

        toggle.setOnClickListener {
            when {
                Bridge.state.value.running -> stopService(Intent(this, BridgeService::class.java))
                Permissions.hasEssential(this) -> startBridge()
                else -> permissionLauncher.launch(Permissions.toRequest())
            }
        }

        // On s'abonne à l'état tant que l'écran est visible (STARTED) ; l'abonnement s'arrête tout seul sinon.
        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.STARTED) {
                Bridge.state.collect { render(it) }
            }
        }
    }

    private fun startBridge() {
        val gpsOn = getSystemService(LocationManager::class.java)?.isProviderEnabled(LocationManager.GPS_PROVIDER) == true
        if (!gpsOn) return say("Active la localisation (GPS) du téléphone.")
        val btOn = getSystemService(BluetoothManager::class.java)?.adapter?.isEnabled == true
        if (!btOn) return say("Active le Bluetooth.")
        say("")
        ContextCompat.startForegroundService(this, Intent(this, BridgeService::class.java))
    }

    private fun say(text: String) {
        Bridge.state.value = Bridge.state.value.copy(message = text)
    }

    private fun render(s: UiState) {
        toggle.text = if (s.running) "Arrêter" else "Démarrer"
        messageText.text = s.message
        gpsText.text = if (s.running) "GPS : ${s.gps}" else ""
        devicesText.text = when {
            !s.running -> ""
            s.devices.isEmpty() -> "Recherche de cartes BikeCopilot…"
            else -> s.devices.joinToString("\n\n") { "${symbol(it.state)} ${it.name}\n     ${it.detail}" }
        }
    }

    private fun symbol(state: LinkState) = when (state) {
        LinkState.READY -> "●"
        LinkState.CONNECTING, LinkState.DISCOVERING -> "◌"
        LinkState.DISCONNECTED -> "○"
        LinkState.INCOMPATIBLE -> "✕"
    }
}
