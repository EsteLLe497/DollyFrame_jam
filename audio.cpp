#include "audio.h"

#include <xaudio2.h>

#include <cstring>
#include <cmath>
#include <vector>

#pragma comment(lib, "xaudio2.lib")

namespace
{
    constexpr double PI_D = 3.14159265358979323846;

    IXAudio2* g_xaudio = nullptr;
    IXAudio2MasteringVoice* g_masterVoice = nullptr;
    IXAudio2SourceVoice* g_sourceVoice = nullptr;
    WAVEFORMATEX g_waveFormat{};
    std::vector<short> g_pcmDataTest;
    std::vector<short> g_pcmDataContact;
    std::vector<short> g_pcmDataScene;
    XAUDIO2_BUFFER g_bufferTest{};
    XAUDIO2_BUFFER g_bufferContact{};
    XAUDIO2_BUFFER g_bufferScene{};
    float g_masterVolume = 0.6f;

    void BuildTone(std::vector<short>& pcmData, XAUDIO2_BUFFER& buffer, float durationSec, float frequency, float amplitude)
    {
        constexpr int sampleRate = 48000;
        const int sampleCount = static_cast<int>(sampleRate * durationSec);

        pcmData.resize(static_cast<size_t>(sampleCount));
        for (int i = 0; i < sampleCount; ++i)
        {
            const double t = static_cast<double>(i) / static_cast<double>(sampleRate);
            const double envelope = 1.0 - (static_cast<double>(i) / static_cast<double>(sampleCount));
            const double sample = sin(2.0 * PI_D * frequency * t) * envelope;
            pcmData[static_cast<size_t>(i)] = static_cast<short>(sample * amplitude);
        }

        g_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        g_waveFormat.nChannels = 1;
        g_waveFormat.nSamplesPerSec = sampleRate;
        g_waveFormat.wBitsPerSample = 16;
        g_waveFormat.nBlockAlign = g_waveFormat.nChannels * g_waveFormat.wBitsPerSample / 8;
        g_waveFormat.nAvgBytesPerSec = g_waveFormat.nSamplesPerSec * g_waveFormat.nBlockAlign;

        buffer.AudioBytes = static_cast<UINT32>(pcmData.size() * sizeof(short));
        buffer.pAudioData = reinterpret_cast<const BYTE*>(pcmData.data());
        buffer.Flags = XAUDIO2_END_OF_STREAM;
    }

    void BuildCues()
    {
        BuildTone(g_pcmDataTest, g_bufferTest, 0.18f, 660.0f, 12000.0f);
        BuildTone(g_pcmDataContact, g_bufferContact, 0.12f, 880.0f, 10000.0f);
        BuildTone(g_pcmDataScene, g_bufferScene, 0.20f, 520.0f, 11000.0f);
    }

    void PlayBuffer(const XAUDIO2_BUFFER& buffer)
    {
        if (!g_sourceVoice)
        {
            return;
        }

        g_sourceVoice->Stop(0);
        g_sourceVoice->FlushSourceBuffers();
        g_sourceVoice->SubmitSourceBuffer(&buffer);
        g_sourceVoice->Start(0);
    }
}

bool Audio_Initialize()
{
    BuildCues();

    HRESULT hr = XAudio2Create(&g_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_xaudio->CreateMasteringVoice(&g_masterVoice);
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_xaudio->CreateSourceVoice(&g_sourceVoice, &g_waveFormat);
    if (FAILED(hr))
    {
        return false;
    }

    Audio_SetMasterVolume(g_masterVolume);
    return true;
}

void Audio_Shutdown()
{
    if (g_sourceVoice)
    {
        g_sourceVoice->DestroyVoice();
        g_sourceVoice = nullptr;
    }
    if (g_masterVoice)
    {
        g_masterVoice->DestroyVoice();
        g_masterVoice = nullptr;
    }
    if (g_xaudio)
    {
        g_xaudio->Release();
        g_xaudio = nullptr;
    }
}

void Audio_Update()
{
}

void Audio_PlayTestTone()
{
    PlayBuffer(g_bufferTest);
}

void Audio_PlayCue(const char* cueName)
{
    if (!cueName)
    {
        return;
    }

    if (std::strcmp(cueName, "test_tone") == 0)
    {
        PlayBuffer(g_bufferTest);
    }
    else if (std::strcmp(cueName, "contact_tone") == 0)
    {
        PlayBuffer(g_bufferContact);
    }
    else if (std::strcmp(cueName, "scene_change") == 0)
    {
        PlayBuffer(g_bufferScene);
    }
}

void Audio_SetMasterVolume(float volume)
{
    g_masterVolume = volume;
    if (g_masterVoice)
    {
        g_masterVoice->SetVolume(g_masterVolume);
    }
}

float Audio_GetMasterVolume()
{
    return g_masterVolume;
}
