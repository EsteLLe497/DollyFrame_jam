#include "pch.h"

#include "audio.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "DxLib.h"

namespace
{
    constexpr double PI_D = 3.14159265358979323846;

    struct CueData
    {
        std::vector<short> pcm;
        int handle = -1;
    };

    CueData g_testCue;
    CueData g_contactCue;
    CueData g_sceneCue;
    WAVEFORMATEX g_waveFormat{};

    float g_masterVolume = 0.6f;
    float g_seVolume = 1.0f;

    // Loaded file cue handles by cue name.
    std::unordered_map<std::string, int> g_fileCues;

    float Clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    bool IsBgmCueName(const std::string& cueName)
    {
        return cueName.find("bgm") != std::string::npos || cueName.find("BGM") != std::string::npos;
    }

    float ResolveCueVolume(const std::string& cueName)
    {
        const float categoryVolume = IsBgmCueName(cueName) ? 1.0f : g_seVolume;
        return Clamp01(g_masterVolume * categoryVolume);
    }

    void ApplyCueVolume(int handle, const std::string& cueName)
    {
        if (handle < 0)
        {
            return;
        }
        SetVolumeSoundMem(static_cast<int>(ResolveCueVolume(cueName) * 10000.0f), handle);
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

    void RefreshAllVolumes()
    {
        ApplyCueVolume(g_testCue.handle, "test_tone");
        ApplyCueVolume(g_contactCue.handle, "contact_tone");
        ApplyCueVolume(g_sceneCue.handle, "scene_change");

        for (const auto& kv : g_fileCues)
        {
            ApplyCueVolume(kv.second, kv.first);
        }
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
        return false;
    }

    g_fileCues.emplace(name, handle);
    ApplyCueVolume(handle, name);
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
