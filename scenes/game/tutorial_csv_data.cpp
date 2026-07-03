// =========================================================
// ファイルの情報[tutorial_csv_data.cpp]
//
// 制作者:Masatora Tanaka		日付：2026/07/01
// =========================================================
#include "pch.h"

#include "tutorial_csv_data.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <utility>

#include "logger.h"

namespace
{
    constexpr size_t kTutorialCsvMinimumColumnCount = 9;

    struct OrderedTutorialPage
    {
        int order = 0;
        TutorialPageData page;
    };

    // CSVの引用符とカンマを考慮して1行を分割します。
    bool parseCsvRow(const std::string& line, std::vector<std::string>& outCells)
    {
        outCells.clear();
        std::string cell;
        bool inQuotes = false;

        for (size_t index = 0; index < line.size(); ++index)
        {
            const char character = line[index];
            if (character == '"')
            {
                if (inQuotes && index + 1 < line.size() && line[index + 1] == '"')
                {
                    cell.push_back('"');
                    ++index;
                }
                else
                {
                    inQuotes = !inQuotes;
                }
                continue;
            }

            if (character == ',' && !inQuotes)
            {
                outCells.push_back(std::move(cell));
                cell.clear();
                continue;
            }
            cell.push_back(character);
        }

        if (inQuotes)
        {
            return false;
        }
        outCells.push_back(std::move(cell));
        return true;
    }

    // CSVで扱いやすい「\n」をゲーム内改行へ変換します。
    std::string decodeEscapedText(const std::string& value)
    {
        std::string decoded;
        decoded.reserve(value.size());
        for (size_t index = 0; index < value.size(); ++index)
        {
            if (value[index] == '\\' && index + 1 < value.size())
            {
                if (value[index + 1] == 'n')
                {
                    decoded.push_back('\n');
                    ++index;
                    continue;
                }
                if (value[index + 1] == '\\')
                {
                    decoded.push_back('\\');
                    ++index;
                    continue;
                }
            }
            decoded.push_back(value[index]);
        }
        return decoded;
    }

    std::string toLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    bool tryParseOrder(const std::string& value, int& outOrder)
    {
        try
        {
            size_t parsedLength = 0;
            outOrder = std::stoi(value, &parsedLength);
            return parsedLength == value.size();
        }
        catch (...)
        {
            return false;
        }
    }
}

// =========================================================
// 指定IDのチュートリアルをCSVから読み込む
// =========================================================
bool loadTutorialPagesFromCsv(
    const std::string& csvPath,
    const std::string& tutorialId,
    std::vector<TutorialPageData>& outPages)
{
    outPages.clear();

    std::ifstream stream(csvPath, std::ios::binary);
    if (!stream.is_open())
    {
        Logger::Error("Tutorial CSV could not be opened: " + csvPath);
        return false;
    }

    std::vector<OrderedTutorialPage> orderedPages;
    std::vector<std::string> cells;
    std::string line;
    int lineNumber = 0;
    while (std::getline(stream, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (lineNumber == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xef &&
            static_cast<unsigned char>(line[1]) == 0xbb &&
            static_cast<unsigned char>(line[2]) == 0xbf)
        {
            line.erase(0, 3);
        }
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        if (!parseCsvRow(line, cells) || cells.size() < kTutorialCsvMinimumColumnCount)
        {
            Logger::Warn(
                "Tutorial CSV row was skipped at line " +
                std::to_string(lineNumber) + ": " + csvPath);
            continue;
        }
        if (toLowerCopy(cells[0]) == "tutorialid" || cells[0] != tutorialId)
        {
            continue;
        }

        OrderedTutorialPage orderedPage;
        if (!tryParseOrder(cells[1], orderedPage.order))
        {
            Logger::Warn(
                "Tutorial CSV order is invalid at line " +
                std::to_string(lineNumber) + ": " + csvPath);
            continue;
        }

        const std::string pageType = toLowerCopy(cells[2]);
        if (pageType == "conversation")
        {
            orderedPage.page.type = TutorialPageType::Conversation;
        }
        else if (pageType == "window")
        {
            orderedPage.page.type = TutorialPageType::Window;
        }
        else
        {
            Logger::Warn(
                "Tutorial CSV type is invalid at line " +
                std::to_string(lineNumber) + ": " + cells[2]);
            continue;
        }

        orderedPage.page.speaker = decodeEscapedText(cells[3]);
        orderedPage.page.portraitPath = cells[4];
        orderedPage.page.title = decodeEscapedText(cells[5]);
        orderedPage.page.text = decodeEscapedText(cells[6]);
        orderedPage.page.confirmText = decodeEscapedText(cells[7]);
        orderedPage.page.contentTextureKey = cells[8];
        if (cells.size() >= 10)
        {
            orderedPage.page.contentVideoPath = cells[9];
        }
        orderedPages.push_back(std::move(orderedPage));
    }

    std::stable_sort(
        orderedPages.begin(),
        orderedPages.end(),
        [](const OrderedTutorialPage& left, const OrderedTutorialPage& right)
        {
            return left.order < right.order;
        });

    outPages.reserve(orderedPages.size());
    for (auto& orderedPage : orderedPages)
    {
        outPages.push_back(std::move(orderedPage.page));
    }

    if (outPages.empty())
    {
        Logger::Error(
            "Tutorial CSV contains no pages for id '" +
            tutorialId + "': " + csvPath);
        return false;
    }
    return true;
}
