package io.github.lstux.blecompanion

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat

/**
 * Android demande l'accord de l'utilisateur pour tout ce qui est sensible. Les règles changent selon la version :
 *  - Android 12+ (API 31) : permissions Bluetooth dédiées (BLUETOOTH_SCAN, BLUETOOTH_CONNECT) + position précise ;
 *  - avant : le Bluetooth est accordé à l'installation, mais le scan BLE exige la position précise.
 *  - Android 13+ (API 33) : POST_NOTIFICATIONS pour que la notification du service soit visible (facultative).
 */
object Permissions {
    /** Sans elles, l'appli ne peut pas fonctionner. */
    private val essential: List<String> = buildList {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            add(Manifest.permission.BLUETOOTH_SCAN)
            add(Manifest.permission.BLUETOOTH_CONNECT)
        }
        add(Manifest.permission.ACCESS_FINE_LOCATION)
    }

    /** Liste à présenter à l'utilisateur (la position approximative doit être demandée avec la précise). */
    fun toRequest(): Array<String> = buildList {
        addAll(essential)
        add(Manifest.permission.ACCESS_COARSE_LOCATION)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) add(Manifest.permission.POST_NOTIFICATIONS)
    }.toTypedArray()

    fun hasEssential(context: Context): Boolean = essential.all {
        ContextCompat.checkSelfPermission(context, it) == PackageManager.PERMISSION_GRANTED
    }
}
