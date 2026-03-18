#include "audio.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <unordered_map>

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

    // 外部ファイル（LoadSoundMem）で作るハンドルを保持するマップ
    std::unordered_map<std::string, int> g_fileCues;

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

    bool CreateCueHandle(CueData& cue)
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

        SetVolumeSoundMem(static_cast<int>(g_masterVolume * 10000.0f), cue.handle);
        return true;
    }

    void PlayHandle(int handle)
    {
        if (handle >= 0)
        {
            PlaySoundMem(handle, DX_PLAYTYPE_BACK, TRUE);
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

    return CreateCueHandle(g_testCue) &&
        CreateCueHandle(g_contactCue) &&
        CreateCueHandle(g_sceneCue);
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

    // 登録された外部ファイルハンドルを削除
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
    PlayHandle(g_testCue.handle);
}

void Audio_PlayCue(const char* cueName)
{
    if (!cueName)
    {
        return;
    }

    // まず外部登録済みのキューを探す
    auto it = g_fileCues.find(cueName);
    if (it != g_fileCues.end() && it->second >= 0)
    {
        PlayHandle(it->second);
        return;
    }

    // 既存の組み込みキュー
    if (std::strcmp(cueName, "test_tone") == 0)
    {
        PlayHandle(g_testCue.handle);
    }
    else if (std::strcmp(cueName, "contact_tone") == 0)
    {
        PlayHandle(g_contactCue.handle);
    }
    else if (std::strcmp(cueName, "scene_change") == 0)
    {
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
        // 既に登録済み
        return true;
    }

    int handle = LoadSoundMem(filePath);
    if (handle < 0)
    {
        return false;
    }
    SetVolumeSoundMem(static_cast<int>(g_masterVolume * 10000.0f), handle);
    g_fileCues.emplace(name, handle);
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
    g_masterVolume = volume;
    if (g_testCue.handle >= 0)
    {
        SetVolumeSoundMem(static_cast<int>(g_masterVolume * 10000.0f), g_testCue.handle);
    }
    if (g_contactCue.handle >= 0)
    {
        SetVolumeSoundMem(static_cast<int>(g_masterVolume * 10000.0f), g_contactCue.handle);
    }
    if (g_sceneCue.handle >= 0)
    {
        SetVolumeSoundMem(static_cast<int>(g_masterVolume * 10000.0f), g_sceneCue.handle);
    }

    // 外部ファイルにも反映
    for (auto& kv : g_fileCues)
    {
        if (kv.second >= 0)
        {
            SetVolumeSoundMem(static_cast<int>(g_masterVolume * 10000.0f), kv.second);
        }
    }
}

float Audio_GetMasterVolume()
{
    return g_masterVolume;
}