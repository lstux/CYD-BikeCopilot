package io.github.lstux.blecompanion

import android.annotation.SuppressLint
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Intent
import android.content.pm.ServiceInfo
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.os.Looper
import android.os.ParcelUuid
import android.os.SystemClock
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import io.github.lstux.blecompanion.proto.BikeProto
import java.util.Locale
import java.util.concurrent.ConcurrentHashMap

/**
 * Le « pont » : un SERVICE DE PREMIER PLAN, c'est-à-dire un morceau de l'appli qui continue de tourner écran éteint,
 * à condition d'afficher une notification permanente.
 *
 * Il fait trois choses :
 *  1. écoute le GPS du téléphone (une position par seconde) ;
 *  2. scanne le BLE à la recherche de cartes qui annoncent le service BikeCopilot, et s'y connecte (une à la fois) ;
 *  3. à chaque position, écrit un message GPS_FIX dans la caractéristique Feed de chaque carte prête.
 *
 * Les permissions sont vérifiées par MainActivity avant le démarrage (d'où l'annotation ci-dessous).
 */
@SuppressLint("MissingPermission")
class BridgeService : Service() {

    private val links = ConcurrentHashMap<String, Link>() // adresse Bluetooth -> liaison
    private var scanner: BluetoothLeScanner? = null
    private var locationManager: LocationManager? = null
    private var running = false
    private var message = ""
    private var gpsText = "en attente du premier point GPS…"

    override fun onBind(intent: Intent?): IBinder? = null // service « démarré », pas « lié »

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (running) return START_NOT_STICKY
        // Cas rare (permission retirée entre l'appui et le démarrage) : on abandonne.
        if (!Permissions.hasEssential(this)) {
            stopSelf()
            return START_NOT_STICKY
        }
        startInForeground()
        running = true

        val adapter = getSystemService(BluetoothManager::class.java)?.adapter
        if (adapter == null || !adapter.isEnabled) {
            message = "Bluetooth désactivé : active-le puis relance."
            publish()
            stopSelf()
            return START_NOT_STICKY
        }

        locationManager = getSystemService(LocationManager::class.java)
        try {
            locationManager?.requestLocationUpdates(
                LocationManager.GPS_PROVIDER, 1000L, 0f, locationListener, Looper.getMainLooper(),
            )
        } catch (e: Exception) {
            message = "GPS indisponible : ${e.message}"
            publish()
            stopSelf()
            return START_NOT_STICKY
        }

        // On ne scanne que les annonces qui contiennent l'UUID du service BikeCopilot.
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(BikeProto.SERVICE_UUID)).build()
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_BALANCED).build()
        scanner = adapter.bluetoothLeScanner
        scanner?.startScan(listOf(filter), settings, scanCallback)

        publish()
        return START_NOT_STICKY
    }

    override fun onDestroy() {
        running = false
        locationManager?.removeUpdates(locationListener)
        try {
            scanner?.stopScan(scanCallback)
        } catch (_: Exception) {
            // Bluetooth coupé entre-temps : rien à arrêter.
        }
        links.values.forEach { it.shutdown() }
        links.clear()
        Bridge.state.value = UiState(running = false, message = message)
        super.onDestroy()
    }

    // ---------------------------------------------------------------- notification

    private fun startInForeground() {
        val channelId = "bridge"
        getSystemService(NotificationManager::class.java).createNotificationChannel(
            NotificationChannel(channelId, "Pont BLE", NotificationManager.IMPORTANCE_LOW),
        )
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE,
        )
        val notification: Notification = NotificationCompat.Builder(this, channelId)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentTitle("BLE-Companion")
            .setContentText("Envoi de la position GPS aux cartes BikeCopilot")
            .setContentIntent(open)
            .setOngoing(true)
            .build()
        ServiceCompat.startForeground(
            this, 1, notification,
            ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION or ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE,
        )
    }

    // ---------------------------------------------------------------- GPS

    // On redéfinit toutes les méthodes : sur Android < 11 certaines sont obligatoires.
    private val locationListener = object : LocationListener {
        override fun onLocationChanged(location: Location) = onLocation(location)
        override fun onProviderEnabled(provider: String) {}
        override fun onProviderDisabled(provider: String) {}

        @Suppress("OVERRIDE_DEPRECATION")
        override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) {}
    }

    private fun onLocation(l: Location) {
        // Position périmée (plus de 5 s) : on se tait, la carte invalidera sa Fix d'elle-même.
        val ageMs = (SystemClock.elapsedRealtimeNanos() - l.elapsedRealtimeNanos) / 1_000_000
        if (ageMs > 5000) return

        // Android donne l'altitude par rapport à l'ellipsoïde WGS84 ; le niveau de la mer (MSL) est dispo depuis Android 14.
        val altitude: Double? = when {
            Build.VERSION.SDK_INT >= 34 && l.hasMslAltitude() -> l.mslAltitudeMeters
            l.hasAltitude() -> l.altitude
            else -> null
        }
        val fix = BikeProto.GpsFix(
            latDeg = l.latitude,
            lonDeg = l.longitude,
            speedKmh = if (l.hasSpeed()) l.speed * 3.6 else null, // m/s -> km/h
            courseDeg = if (l.hasBearing()) l.bearing.toDouble() else null,
            altitudeM = altitude,
            accuracyM = if (l.hasAccuracy()) l.accuracy.toDouble() else null,
            satellites = l.extras?.getInt("satellites", 0) ?: 0,
            utcSeconds = l.time / 1000, // l.time est en millisecondes UTC
        )
        val bytes = BikeProto.encodeGpsFix(fix)
        links.values.forEach { if (it.state == LinkState.READY) it.send(bytes) }

        gpsText = String.format(
            Locale.US, "%.5f, %.5f · %.1f km/h · ±%.0f m",
            l.latitude, l.longitude, if (l.hasSpeed()) l.speed * 3.6 else 0.0, if (l.hasAccuracy()) l.accuracy else 0f,
        )
        publish()
    }

    // ---------------------------------------------------------------- scan

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val address = result.device.address
            val existing = links[address]
            // Déjà connectée (ou jugée incompatible) : on ignore. Seule une carte « perdue » est reprise.
            if (existing != null && existing.state != LinkState.DISCONNECTED) return
            // Android supporte mal plusieurs connexions simultanées en cours d'établissement : une à la fois.
            if (links.values.any { it.state == LinkState.CONNECTING || it.state == LinkState.DISCOVERING }) return

            val name = result.scanRecord?.deviceName ?: result.device.name ?: address
            val link = Link(result.device, name)
            links[address] = link
            link.gatt = result.device.connectGatt(this@BridgeService, false, link, BluetoothDevice.TRANSPORT_LE)
            publish()
        }

        override fun onScanFailed(errorCode: Int) {
            message = "Échec du scan Bluetooth (code $errorCode)"
            publish()
        }
    }

    // ---------------------------------------------------------------- une liaison GATT = une carte

    /**
     * GATT est le protocole de « services et caractéristiques » du BLE. Séquence pour chaque carte :
     * connexion -> négociation de la MTU (taille max d'un paquet) -> découverte des services -> lecture de Device Info
     * -> prête. Chaque étape est asynchrone : Android appelle la méthode « on… » correspondante quand elle est terminée.
     * Ces rappels arrivent sur un autre thread que l'écran, d'où les champs @Volatile.
     */
    private inner class Link(val device: BluetoothDevice, val name: String) : BluetoothGattCallback() {
        @Volatile var gatt: BluetoothGatt? = null
        @Volatile var state = LinkState.CONNECTING
        @Volatile private var detail = "connexion…"
        @Volatile private var mtu = 23 // valeur par défaut du BLE
        @Volatile private var feed: BluetoothGattCharacteristic? = null
        @Volatile private var sent = 0
        private var boardName = ""

        fun toUi() = DeviceUi(device.address, name, state, detail)

        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED && status == BluetoothGatt.GATT_SUCCESS) {
                state = LinkState.DISCOVERING
                detail = "négociation de la MTU…"
                if (!g.requestMtu(BikeProto.REQUESTED_MTU)) startDiscovery(g)
            } else {
                // Déconnexion (voulue, perte de signal, carte éteinte, ou échec : status 133 est fréquent sous Android).
                if (state != LinkState.INCOMPATIBLE) {
                    state = LinkState.DISCONNECTED
                    detail = "déconnectée (code $status) — recherche…"
                }
                feed = null
                g.close()
            }
            publish()
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            this.mtu = mtu
            startDiscovery(g)
        }

        private fun startDiscovery(g: BluetoothGatt) {
            detail = "découverte des services…"
            if (!g.discoverServices()) reject(g, "découverte impossible")
            publish()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val service = g.getService(BikeProto.SERVICE_UUID)
            val info = service?.getCharacteristic(BikeProto.INFO_UUID)
            val feedChar = service?.getCharacteristic(BikeProto.FEED_UUID)
            if (status != BluetoothGatt.GATT_SUCCESS || info == null || feedChar == null) {
                reject(g, "service BikeCopilot incomplet")
            } else {
                feed = feedChar
                detail = "lecture de Device Info…"
                if (!g.readCharacteristic(info)) reject(g, "lecture de Device Info impossible")
            }
            publish()
        }

        // Android 13+ fournit la valeur directement ; avant, elle est dans characteristic.value.
        // On redéfinit les deux versions (sans appeler super) pour qu'une seule soit appelée selon la version d'Android.
        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) =
            onInfo(g, value, status)

        @Suppress("DEPRECATION", "OVERRIDE_DEPRECATION")
        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) =
            onInfo(g, c.value ?: ByteArray(0), status)

        private fun onInfo(g: BluetoothGatt, value: ByteArray, status: Int) {
            val info = if (status == BluetoothGatt.GATT_SUCCESS) BikeProto.parseDeviceInfo(value) else null
            when {
                info == null -> reject(g, "Device Info illisible")
                info.major != BikeProto.MAJOR -> reject(g, "protocole v${info.major}.${info.minor} non géré")
                !info.accepts(BikeProto.MSG_GPS_FIX) -> reject(g, "n'accepte pas la position GPS")
                mtu < BikeProto.MIN_MTU -> reject(g, "MTU trop petite ($mtu)")
                else -> {
                    boardName = info.boardName
                    state = LinkState.READY
                    detail = "prête · $boardName · MTU $mtu"
                }
            }
            publish()
        }

        /** Carte inutilisable : on la garde dans la liste (en rouge) pour ne pas se reconnecter en boucle. */
        private fun reject(g: BluetoothGatt, reason: String) {
            state = LinkState.INCOMPATIBLE
            detail = reason
            g.disconnect() // onConnectionStateChange fermera la connexion
        }

        fun send(message: ByteArray) {
            val g = gatt ?: return
            val c = feed ?: return
            // « Write without response » : pas d'accusé de réception, donc rapide et sans file d'attente à gérer.
            // Une position manquée n'est pas grave : la suivante arrive une seconde plus tard.
            val ok = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                g.writeCharacteristic(c, message, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) ==
                    BluetoothStatusCodes.SUCCESS
            } else {
                legacyWrite(g, c, message)
            }
            if (ok) {
                sent++
                detail = "prête · $boardName · MTU $mtu · $sent positions envoyées"
            }
        }

        // Avant Android 13 : on dépose la valeur dans la caractéristique puis on demande l'écriture.
        @Suppress("DEPRECATION")
        private fun legacyWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, message: ByteArray): Boolean {
            c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            c.value = message
            return g.writeCharacteristic(c)
        }

        fun shutdown() {
            gatt?.let {
                it.disconnect()
                it.close()
            }
            gatt = null
        }
    }

    private fun publish() {
        Bridge.state.value = UiState(
            running = running,
            message = message,
            gps = gpsText,
            devices = links.values.sortedBy { it.name }.map { it.toUi() },
        )
    }
}
