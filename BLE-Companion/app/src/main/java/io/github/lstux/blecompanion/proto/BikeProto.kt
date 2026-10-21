package io.github.lstux.blecompanion.proto

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID
import kotlin.math.ceil
import kotlin.math.roundToInt
import kotlin.math.roundToLong

/**
 * Protocole BLE BikeCopilot v1 — côté téléphone.
 * Spécification : common/ble/PROTOCOL.md (à la racine du dépôt). Le décodage côté carte est dans common/src/ble_proto.h.
 *
 * Ce fichier n'utilise AUCUNE classe Android : il se teste sur un simple PC (voir BikeProtoTest).
 */
object BikeProto {
    val SERVICE_UUID: UUID = UUID.fromString("0a2b0001-ec15-422d-8e42-7dfd3f9bff50")
    val INFO_UUID: UUID = UUID.fromString("0a2b0002-ec15-422d-8e42-7dfd3f9bff50")
    val FEED_UUID: UUID = UUID.fromString("0a2b0003-ec15-422d-8e42-7dfd3f9bff50")
    val EVENT_UUID: UUID = UUID.fromString("0a2b0004-ec15-422d-8e42-7dfd3f9bff50") // réservé, inutilisé en v1

    const val MAJOR = 1
    const val MSG_GPS_FIX = 0x01

    /** MTU demandée à la connexion, et minimum acceptable (22 octets de message + 3 d'en-tête ATT). */
    const val REQUESTED_MTU = 247
    const val MIN_MTU = 25

    private const val GPS_PAYLOAD_LEN = 20

    /** Une position du téléphone. `null` = inconnu. */
    class GpsFix(
        val latDeg: Double,
        val lonDeg: Double,
        val speedKmh: Double? = null,
        val courseDeg: Double? = null,
        val altitudeM: Double? = null,
        val accuracyM: Double? = null,
        val satellites: Int = 0,
        /** Secondes Unix UTC ; 0 = inconnu. */
        val utcSeconds: Long = 0,
    )

    /** Message GPS_FIX complet (en-tête de 2 octets + 20 octets), prêt à être écrit dans la caractéristique Feed. */
    fun encodeGpsFix(fix: GpsFix): ByteArray {
        val b = ByteBuffer.allocate(2 + GPS_PAYLOAD_LEN).order(ByteOrder.LITTLE_ENDIAN)
        b.put(MSG_GPS_FIX.toByte())
        b.put(GPS_PAYLOAD_LEN.toByte())
        b.putInt((fix.latDeg * 1e7).roundToLong().toInt())
        b.putInt((fix.lonDeg * 1e7).roundToLong().toInt())

        // vitesse en 0,1 km/h (0xFFFF = inconnue)
        val speed = fix.speedKmh?.let { (it * 10).roundToInt().coerceIn(0, 0xFFFE) } ?: 0xFFFF
        b.putShort(speed.toShort())

        // cap en 0,1 ° dans 0..3599 (0xFFFF = inconnu)
        val course = fix.courseDeg?.let { (((it % 360.0) + 360.0) % 360.0 * 10).roundToInt() % 3600 } ?: 0xFFFF
        b.putShort(course.toShort())

        // altitude en mètres (-32768 = inconnue)
        val alt = fix.altitudeM?.let { it.roundToInt().coerceIn(-32767, 32767) } ?: -32768
        b.putShort(alt.toShort())

        // précision en mètres, arrondie vers le haut (255 = inconnue)
        val acc = fix.accuracyM?.let { ceil(it).toInt().coerceIn(0, 254) } ?: 255
        b.put(acc.toByte())

        b.put(fix.satellites.coerceIn(0, 255).toByte())
        b.putInt((fix.utcSeconds and 0xFFFFFFFFL).toInt())
        return b.array()
    }

    /** Contenu de la caractéristique Device Info. */
    class DeviceInfo(val major: Int, val minor: Int, val board: Int, val feeds: Set<Int>) {
        fun accepts(type: Int) = type in feeds
        val boardName: String
            get() = when (board) {
                1 -> "CYD"
                2 -> "LilyGO T4"
                3 -> "Waveshare C6"
                else -> "carte inconnue"
            }
    }

    /** `null` si la valeur est trop courte pour être une Device Info valide. */
    fun parseDeviceInfo(v: ByteArray): DeviceInfo? {
        if (v.size < 4) return null
        val n = v[3].toInt() and 0xFF
        if (v.size < 4 + n) return null
        val feeds = (0 until n).map { v[4 + it].toInt() and 0xFF }.toSet()
        return DeviceInfo(v[0].toInt() and 0xFF, v[1].toInt() and 0xFF, v[2].toInt() and 0xFF, feeds)
    }
}
