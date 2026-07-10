#include "pch.h"

#include "photo_log.h"

#include <algorithm>
#include <array>

namespace
{
    std::array<PhotoCaptureState, kPhotoLogCapacity> g_photoLogEntries;
    int g_photoLogCount = 0;
    int g_photoLogNextIndex = 0; // Ÿ‚Éã‘‚«‚·‚éˆÊ’u(–”t‚Íˆê”ÔŒÃ‚¢‚à‚Ì‚ğã‘‚«)
}

void PhotoLog_Reset()
{
    for (auto& entry : g_photoLogEntries)
    {
        entry = PhotoCaptureState{};
    }
    g_photoLogCount = 0;
    g_photoLogNextIndex = 0;
}

void PhotoLog_Add(const PhotoCaptureState& capture)
{
    g_photoLogEntries[static_cast<size_t>(g_photoLogNextIndex)] = capture;
    g_photoLogNextIndex = (g_photoLogNextIndex + 1) % kPhotoLogCapacity;
    g_photoLogCount = std::min(g_photoLogCount + 1, kPhotoLogCapacity);
}

int PhotoLog_GetCount()
{
    return g_photoLogCount;
}

const PhotoCaptureState& PhotoLog_GetEntry(int index)
{
    // –”t‚Å‚È‚¯‚ê‚Î0”Ô‚©‚ç‡‚ÉA–”t‚È‚çˆê”ÔŒÃ‚¢‚à‚Ì(Ÿ‚Éã‘‚«‚³‚ê‚éˆÊ’u)‚©‚ç‡‚É•À‚×‚é
    const int start = g_photoLogCount < kPhotoLogCapacity ? 0 : g_photoLogNextIndex;
    const int actualIndex = (start + index) % kPhotoLogCapacity;
    return g_photoLogEntries[static_cast<size_t>(actualIndex)];
}