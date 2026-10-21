package io.github.lstux.blecompanion

import android.bluetooth.BluetoothManager
import android.content.Intent
import android.location.LocationManager
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
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

        // Depuis Android 15, l'appli est dessinée SOUS la barre d'état et la barre de navigation.
        // Il faut donc ajouter nous-mêmes des marges égales à la taille de ces barres (et de l'encoche).
        val root = findViewById<View>(R.id.root)
        val margin = (16 * resources.displayMetrics.density).toInt()
        ViewCompat.setOnApplyWindowInsetsListener(root) { v, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            v.setPadding(bars.left + margin, bars.top + margin, bars.right + margin, bars.bottom + margin)
            insets
        }

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
        messageText.visibility = if (s.message.isEmpty()) View.GONE else View.VISIBLE
        gpsText.text = if (s.running) s.gps else "Arrêté"
        devicesText.text = when {
            !s.running -> "Appuie sur Démarrer pour envoyer la position GPS aux cartes BikeCopilot à proximité."
            s.devices.isEmpty() -> "Recherche de cartes BikeCopilot…"
            else -> s.devices.joinToString("\n\n") { "${symbol(it.state)}  ${it.name}\n      ${it.detail}" }
        }
    }

    private fun symbol(state: LinkState) = when (state) {
        LinkState.READY -> "●"
        LinkState.CONNECTING, LinkState.DISCOVERING -> "◌"
        LinkState.DISCONNECTED -> "○"
        LinkState.INCOMPATIBLE -> "✕"
    }
}
