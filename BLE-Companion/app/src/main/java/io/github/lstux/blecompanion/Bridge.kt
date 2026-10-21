package io.github.lstux.blecompanion

import kotlinx.coroutines.flow.MutableStateFlow

/** État d'une liaison avec une carte. */
enum class LinkState { CONNECTING, DISCOVERING, READY, DISCONNECTED, INCOMPATIBLE }

/** Ce que l'écran affiche pour une carte. */
data class DeviceUi(val address: String, val name: String, val state: LinkState, val detail: String)

/** Tout ce que l'écran a besoin de savoir. Immuable : le service en publie un nouveau à chaque changement. */
data class UiState(
    val running: Boolean = false,
    val message: String = "",
    val gps: String = "",
    val devices: List<DeviceUi> = emptyList(),
)

/**
 * Point de rencontre entre le service (qui travaille en arrière-plan) et l'écran (qui affiche).
 * Un StateFlow est une « variable observable » : l'écran s'abonne et est prévenu à chaque nouvelle valeur.
 * C'est la solution la plus simple pour une appli à un seul processus ; on pourra passer à un service « lié » plus tard.
 */
object Bridge {
    val state = MutableStateFlow(UiState())
}
