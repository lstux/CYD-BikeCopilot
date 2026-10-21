package io.github.lstux.blecompanion.proto

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/** Mêmes vecteurs que common/ble/PROTOCOL.md et common/tools/ble_proto_test.cpp. */
class BikeProtoTest {
    private fun hex(s: String) = s.trim().split(" ").map { it.toInt(16).toByte() }.toByteArray()

    @Test
    fun gpsFixComplet() {
        val fix = BikeProto.GpsFix(
            latDeg = 48.8566, lonDeg = 2.3522,
            speedKmh = 23.4, courseDeg = 187.5, altitudeM = 35.0, accuracyM = 6.0,
            satellites = 12, utcSeconds = 1792578907L, // 2026-10-21 10:35:07 UTC
        )
        assertArrayEquals(
            hex("01 14 f0 ec 1e 1d d0 ea 66 01 ea 00 53 07 23 00 06 0c 5b 95 d8 6a"),
            BikeProto.encodeGpsFix(fix),
        )
    }

    @Test
    fun gpsFixToutInconnu() {
        val fix = BikeProto.GpsFix(latDeg = 48.8566, lonDeg = 2.3522)
        assertArrayEquals(
            hex("01 14 f0 ec 1e 1d d0 ea 66 01 ff ff ff ff 00 80 ff 00 00 00 00 00"),
            BikeProto.encodeGpsFix(fix),
        )
    }

    @Test
    fun valeursLimitesEtCapNegatif() {
        val m = BikeProto.encodeGpsFix(
            BikeProto.GpsFix(0.0, 0.0, speedKmh = 1e6, courseDeg = -90.0, altitudeM = 99999.0, accuracyM = 1000.0, satellites = 999),
        )
        // vitesse saturée à 0xFFFE ; cap -90° → 270,0° = 2700 = 0x0A8C ; altitude saturée à 32767 ;
        // précision saturée à 254 ; satellites saturés à 255
        assertArrayEquals(hex("01 14 00 00 00 00 00 00 00 00 fe ff 8c 0a ff 7f fe ff 00 00 00 00"), m)
    }

    @Test
    fun deviceInfo() {
        val info = BikeProto.parseDeviceInfo(hex("01 00 02 01 01"))
        assertNotNull(info)
        assertEquals(1, info!!.major)
        assertEquals("LilyGO T4", info.boardName)
        assertTrue(info.accepts(BikeProto.MSG_GPS_FIX))
        assertNull(BikeProto.parseDeviceInfo(hex("01 00 02")))       // trop court
        assertNull(BikeProto.parseDeviceInfo(hex("01 00 02 02 01"))) // annonce 2 types, un seul présent
    }
}
