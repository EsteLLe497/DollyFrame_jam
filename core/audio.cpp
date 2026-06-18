#include "pch.h"

#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "DxLib.h"
#include "logger.h"

namespace
{
    constexpr double PI_D = 3.14159265358979323846;

    struct CueData
    {
        std::vector<short> pcm;
        int handle = -1;
    };

    struct BgmFadeState
    {
        bool active = false;
        std::string fromCueName;
        std::string toCueName;
        float elapsed = 0.0f;
        float duration = 0.0f;
        int lastTick = 0;
    };

    CueData g_testCue;
    CueData g_contactCue;
    CueData g_sceneCue;
    WAVEFORMATEX g_waveFormat{};

    float g_masterVolume = 0.8f;
    float g_seVolume = 0.8f;

    // Loaded file cue handles by cue name.
    std::unordered_map<std::string, int> g_fileCues;
    std::string g_currentBgmCueName;
    BgmFadeState g_bgmFade;

    float Clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    bool IsBgmCueName(const std::string& cueName)
    {
        return cueName.find("bgm") != std::string::npos || cueName.find("BGM") != std::string::npos;
    }

    float ResolveCueGain(const std::string& cueName)
    {
        // 中ボス1SEは素材の平均音量が小さいため、BGMに埋もれないよう少し持ち上げる。
        if (cueName.find("boss_forest_") == 0)
        {
            return 1.8f;
        }
        return 1.0f;
    }

    float ResolveCueVolume(const std::string& cueName)
    {
        const float categoryVolume = IsBgmCueName(cueName) ? 1.0f : g_seVolume;
        return Clamp01(g_masterVolume * categoryVolume * ResolveCueGain(cueName));
    }

    void ApplyCueVolumeScaled(int handle, const std::string& cueName, float volumeScale)
    {
        if (handle < 0)
        {
            return;
        }
        const float resolvedVolume = ResolveCueVolume(cueName) * Clamp01(volumeScale);
        SetVolumeSoundMem(static_cast<int>(resolvedVolume * 10000.0f), handle);
    }

    void ApplyCueVolume(int handle, const std::string& cueName)
    {
        ApplyCueVolumeScaled(handle, cueName, 1.0f);
    }

    void BuildTone(CueData& cue, float durationSec, float frequency, float amplitude)
    {
        constexpr int sampleRate = 48000;
        const int sampleCount = static_cast<int>(sampleRate * durationSec);

        cue.pcm.resize(static_cast<size_t>(sampleCount));
        for (int i = 0; i < sampleCount; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(sampleRate);
            const double envelope = 1.0 - (static_cast<double>(i) / static_cast<double>(sampleCount));
            const double sample = std::sin(2.0 * PI_D * frequency * t) * envelope;
            cue.pcm[static_cast<size_t>(i)] = static_cast<short>(sample * amplitude);
        }
    }

    bool CreateCueHandle(CueData& cue, const std::string& cueName)
    {
        cue.handle = LoadSoundMemByMemImage2(
            cue.pcm.data(),
            cue.pcm.size() * sizeof(short),
            &g_waveFormat,
            sizeof(g_waveFormat));
        if (cue.handle < 0)
        {
            return false;
        }

        ApplyCueVolume(cue.handle, cueName);
        return true;
    }

    void PlayHandle(int handle)
    {
        if (handle >= 0)
        {
            PlaySoundMem(handle, DX_PLAYTYPE_BACK, TRUE);
        }
    }

    void PlayLoopHandle(int handle)
    {
        if (handle >= 0)
        {
            PlaySoundMem(handle, DX_PLAYTYPE_LOOP, TRUE);
        }
    }

    void StopBgmCue(const std::string& cueName)
    {
        auto it = g_fileCues.find(cueName);
        if (it != g_fileCues.end() && it->second >= 0)
        {
            StopSoundMem(it->second);
        }
    }

    void FinishBgmFade()
    {
        if (!g_bgmFade.fromCueName.empty() && g_bgmFade.fromCueName != g_bgmFade.toCueName)
        {
            StopBgmCue(g_bgmFade.fromCueName);
        }

        auto it = g_fileCues.find(g_bgmFade.toCueName);
        if (it != g_fileCues.end() && it->second >= 0)
        {
            ApplyCueVolume(it->second, g_bgmFade.toCueName);
        }

        g_currentBgmCueName = g_bgmFade.toCueName;
        g_bgmFade = BgmFadeState{};
    }

    void ApplyBgmFadeVolumes()
    {
        if (!g_bgmFade.active)
        {
            return;
        }

        const float fadeRate = Clamp01(g_bgmFade.elapsed / std::max(0.001f, g_bgmFade.duration));

        auto fromIt = g_fileCues.find(g_bgmFade.fromCueName);
        if (fromIt != g_fileCues.end() && fromIt->second >= 0)
        {
            ApplyCueVolumeScaled(fromIt->second, g_bgmFade.fromCueName, 1.0f - fadeRate);
        }

        auto toIt = g_fileCues.find(g_bgmFade.toCueName);
        if (toIt != g_fileCues.end() && toIt->second >= 0)
        {
            ApplyCueVolumeScaled(toIt->second, g_bgmFade.toCueName, fadeRate);
        }
    }

    void RefreshAllVolumes()
    {
        ApplyCueVolume(g_testCue.handle, "test_tone");
        ApplyCueVolume(g_contactCue.handle, "contact_tone");
        ApplyCueVolume(g_sceneCue.handle, "scene_change");

        for (const auto& kv : g_fileCues)
        {
            ApplyCueVolume(kv.second, kv.first);
        }
        ApplyBgmFadeVolumes();
    }
}

bool Audio_Initialize()
{
    g_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    g_waveFormat.nChannels = 1;
    g_waveFormat.nSamplesPerSec = 48000;
    g_waveFormat.wBitsPerSample = 16;
    g_waveFormat.nBlockAlign = g_waveFormat.nChannels * g_waveFormat.wBitsPerSample / 8;
    g_waveFormat.nAvgBytesPerSec = g_waveFormat.nSamplesPerSec * g_waveFormat.nBlockAlign;

    BuildTone(g_testCue, 0.18f, 660.0f, 12000.0f);
    BuildTone(g_contactCue, 0.12f, 880.0f, 10000.0f);
    BuildTone(g_sceneCue, 0.20f, 520.0f, 11000.0f);

    return CreateCueHandle(g_testCue, "test_tone") &&
        CreateCueHandle(g_contactCue, "contact_tone") &&
        CreateCueHandle(g_sceneCue, "scene_change");
}

void Audio_Shutdown()
{
    Audio_StopBgm();

    if (g_testCue.handle >= 0)
    {
        DeleteSoundMem(g_testCue.handle);
        g_testCue.handle = -1;
    }
    if (g_contactCue.handle >= 0)
    {
        DeleteSoundMem(g_contactCue.handle);
        g_contactCue.handle = -1;
    }
    if (g_sceneCue.handle >= 0)
    {
        DeleteSoundMem(g_sceneCue.handle);
        g_sceneCue.handle = -1;
    }

    for (auto& kv : g_fileCues)
    {
        if (kv.second >= 0)
        {
            DeleteSoundMem(kv.second);
        }
    }
    g_fileCues.clear();
}

void Audio_Update()
{
    if (!g_bgmFade.active)
    {
        return;
    }

    const int now = GetNowCount();
    const float deltaTime = g_bgmFade.lastTick > 0
        ? static_cast<float>(std::max(0, now - g_bgmFade.lastTick)) / 1000.0f
        : 0.0f;
    g_bgmFade.lastTick = now;
    g_bgmFade.elapsed += deltaTime;

    ApplyBgmFadeVolumes();
    if (g_bgmFade.elapsed >= g_bgmFade.duration)
    {
        FinishBgmFade();
    }
}

void Audio_PlayTestTone()
{
    ApplyCueVolume(g_testCue.handle, "test_tone");
    PlayHandle(g_testCue.handle);
}

void Audio_PlayCue(const char* cueName)
{
    if (!cueName)
    {
        return;
    }

    const std::string cue(cueName);

    auto it = g_fileCues.find(cue);
    if (it != g_fileCues.end() && it->second >= 0)
    {
        ApplyCueVolume(it->second, cue);
        PlayHandle(it->second);
        return;
    }

    if (cue == "test_tone")
    {
        ApplyCueVolume(g_testCue.handle, cue);
        PlayHandle(g_testCue.handle);
    }
    else if (cue == "contact_tone")
    {
        ApplyCueVolume(g_contactCue.handle, cue);
        PlayHandle(g_contactCue.handle);
    }
    else if (cue == "scene_change")
    {
        ApplyCueVolume(g_sceneCue.handle, cue);
        PlayHandle(g_sceneCue.handle);
    }
    else
    {
        Logger::Warn(std::string("Missing sound cue: ") + cue);
    }
}

void Audio_StopCue(const char* cueName)
{
    if (!cueName)
    {
        return;
    }

    const std::string cue(cueName);
    auto it = g_fileCues.find(cue);
    if (it != g_fileCues.end() && it->second >= 0)
    {
        StopSoundMem(it->second);
        return;
    }

    Logger::Warn(std::string("Missing sound cue stop request: ") + cue);
}

void Audio_PlayBgmCue(const char* cueName)
{
    if (!cueName)
    {
        return;
    }

    const std::string cue(cueName);
    auto it = g_fileCues.find(cue);
    if (it == g_fileCues.end() || it->second < 0)
    {
        Logger::Warn(std::string("Missing BGM cue: ") + cue);
        return;
    }

    if (g_currentBgmCueName == cue)
    {
        if (g_bgmFade.active)
        {
            if (!g_bgmFade.fromCueName.empty() && g_bgmFade.fromCueName != cue)
            {
                StopBgmCue(g_bgmFade.fromCueName);
            }
            g_bgmFade = BgmFadeState{};
        }
        ApplyCueVolume(it->second, cue);
        return;
    }

    g_bgmFade = BgmFadeState{};
    Audio_StopBgm();
    g_currentBgmCueName = cue;
    ApplyCueVolume(it->second, cue);
    PlayLoopHandle(it->second);
}

void Audio_CrossFadeBgmCue(const char* cueName, float durationSeconds)
{
    if (!cueName)
    {
        return;
    }

    const std::string cue(cueName);
    auto toIt = g_fileCues.find(cue);
    if (toIt == g_fileCues.end() || toIt->second < 0)
    {
        Logger::Warn(std::string("Missing crossfade BGM cue: ") + cue);
        return;
    }

    if (durationSeconds <= 0.0f)
    {
        Audio_PlayBgmCue(cueName);
        return;
    }

    if (g_currentBgmCueName == cue && !g_bgmFade.active)
    {
        ApplyCueVolume(toIt->second, cue);
        return;
    }

    if (g_bgmFade.active && g_bgmFade.toCueName == cue)
    {
        return;
    }

    if (g_bgmFade.active && !g_bgmFade.fromCueName.empty() && g_bgmFade.fromCueName != g_bgmFade.toCueName)
    {
        StopBgmCue(g_bgmFade.fromCueName);
    }

    const std::string fromCue = g_bgmFade.active ? g_bgmFade.toCueName : g_currentBgmCueName;
    if (!g_bgmFade.active && fromCue.empty())
    {
        g_currentBgmCueName = cue;
        ApplyCueVolumeScaled(toIt->second, cue, 0.0f);
        PlayLoopHandle(toIt->second);
    }
    else
    {
        ApplyCueVolumeScaled(toIt->second, cue, 0.0f);
        PlayLoopHandle(toIt->second);
    }

    g_bgmFade.active = true;
    g_bgmFade.fromCueName = fromCue;
    g_bgmFade.toCueName = cue;
    g_bgmFade.elapsed = 0.0f;
    g_bgmFade.duration = std::max(0.001f, durationSeconds);
    g_bgmFade.lastTick = GetNowCount();
    g_currentBgmCueName = cue;
    ApplyBgmFadeVolumes();
}

void Audio_StopBgm()
{
    if (g_currentBgmCueName.empty())
    {
        return;
    }

    auto it = g_fileCues.find(g_currentBgmCueName);
    if (it != g_fileCues.end() && it->second >= 0)
    {
        StopSoundMem(it->second);
    }
    if (g_bgmFade.active && !g_bgmFade.fromCueName.empty() && g_bgmFade.fromCueName != g_currentBgmCueName)
    {
        StopBgmCue(g_bgmFade.fromCueName);
    }
    g_bgmFade = BgmFadeState{};
    g_currentBgmCueName.clear();
}

bool Audio_LoadCueFromFile(const char* cueName, const char* filePath)
{
    if (!cueName || !filePath)
    {
        return false;
    }

    const std::string name(cueName);
    if (g_fileCues.find(name) != g_fileCues.end())
    {
        return true;
    }

    int handle = LoadSoundMem(filePath);
    if (handle < 0)
    {
        Logger::Warn(std::string("Failed to load sound cue: ") + name + " from " + filePath);
        return false;
    }

    g_fileCues.emplace(name, handle);
    ApplyCueVolume(handle, name);
    Logger::Info(std::string("Loaded sound cue: ") + name + " from " + filePath);
    return true;
}

void Audio_UnloadCue(const char* cueName)
{
    if (!cueName)
    {
        return;
    }

    auto it = g_fileCues.find(cueName);
    if (it == g_fileCues.end())
    {
        return;
    }

    if (it->second >= 0)
    {
        if (g_currentBgmCueName == cueName)
        {
            StopSoundMem(it->second);
            g_currentBgmCueName.clear();
        }
        DeleteSoundMem(it->second);
    }
    g_fileCues.erase(it);
}

void Audio_SetMasterVolume(float volume)
{
    g_masterVolume = Clamp01(volume);
    RefreshAllVolumes();
}

float Audio_GetMasterVolume()
{
    return g_masterVolume;
}

void Audio_SetSeVolume(float volume)
{
    g_seVolume = Clamp01(volume);
    RefreshAllVolumes();
}

float Audio_GetSeVolume()
{
    return g_seVolume;
}
