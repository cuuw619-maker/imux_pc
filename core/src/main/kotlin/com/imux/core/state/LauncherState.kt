package com.imux.core.state
import com.imux.core.model.InstallationProfile
sealed interface LauncherUiState {
    data object Loading : LauncherUiState
    data class Ready(val profile: InstallationProfile, val runtimeAvailable: Boolean) : LauncherUiState
    data class Launching(val profile: InstallationProfile) : LauncherUiState
    data class Running(val profile: InstallationProfile, val pid: Long?) : LauncherUiState
    data class Error(val message: String) : LauncherUiState
}
