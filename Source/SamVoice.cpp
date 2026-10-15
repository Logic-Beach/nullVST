#include "SamVoice.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

extern "C"
{
#include "sam.h"
#include "reciter.h"
#include "render.h"
    int debug = 0;
}

namespace
{
    void appendPiece (char* dst, int& n, int cap, const char* word)
    {
        if (word == nullptr || word[0] == 0 || n >= cap)
            return;
        if (n > 0 && dst[n - 1] != ' ')
        {
            if (n + 1 >= cap)
                return;
            dst[n++] = ' ';
        }
        for (int i = 0; word[i] != 0 && n < cap; ++i)
            dst[n++] = word[i];
    }

    void sayUnderThousand (char* dst, int& n, int cap, int value)
    {
        static const char* ones[] = {
            "", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
            "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN", "SIXTEEN",
            "SEVENTEEN", "EIGHTEEN", "NINETEEN"
        };
        static const char* tens[] = {
            "", "", "TWENTY", "THIRTY", "FORTY", "FIFTY", "SIXTY", "SEVENTY", "EIGHTY", "NINETY"
        };
        if (value >= 100)
        {
            appendPiece (dst, n, cap, ones[value / 100]);
            appendPiece (dst, n, cap, "HUNDRED");
            value %= 100;
        }
        if (value >= 20)
        {
            appendPiece (dst, n, cap, tens[value / 10]);
            if (value % 10 != 0)
                appendPiece (dst, n, cap, ones[value % 10]);
        }
        else if (value > 0)
            appendPiece (dst, n, cap, ones[value]);
    }

    void sayCardinal (char* dst, int& n, int cap, unsigned long long value)
    {
        if (value == 0)
        {
            appendPiece (dst, n, cap, "ZERO");
            return;
        }
        if (value >= 1000000000ull)
        {
            sayUnderThousand (dst, n, cap, (int) (value / 1000000000ull));
            appendPiece (dst, n, cap, "BILLION");
            value %= 1000000000ull;
        }
        if (value >= 1000000ull)
        {
            sayUnderThousand (dst, n, cap, (int) (value / 1000000ull));
            appendPiece (dst, n, cap, "MILLION");
            value %= 1000000ull;
        }
        if (value >= 1000ull)
        {
            sayUnderThousand (dst, n, cap, (int) (value / 1000ull));
            appendPiece (dst, n, cap, "THOUSAND");
            value %= 1000ull;
        }
        if (value > 0)
            sayUnderThousand (dst, n, cap, (int) value);
    }

    void spellDigits (char* dst, int& n, int cap, const char* digits, int count)
    {
        static const char* names[] = {
            "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"
        };
        for (int i = 0; i < count; ++i)
        {
            const int d = digits[i] - '0';
            if (d >= 0 && d <= 9)
                appendPiece (dst, n, cap, names[d]);
        }
    }

    // SAM's reciter only has letter rules. Digit runs become words first.
    void speakNumbers (const char* src, char* dst, int cap)
    {
        int n = 0;
        for (int i = 0; src[i] != 0 && n < cap; )
        {
            const unsigned char c = (unsigned char) src[i];
            const bool minus = c == '-'
                && src[i + 1] >= '0' && src[i + 1] <= '9'
                && (i == 0 || src[i - 1] == ' ');
            if ((c >= '0' && c <= '9') || minus)
            {
                if (minus)
                {
                    appendPiece (dst, n, cap, "MINUS");
                    ++i;
                }
                const int start = i;
                while (src[i] >= '0' && src[i] <= '9' && i - start < 18)
                    ++i;
                const int digits = i - start;
                const bool leadZero = digits > 1 && src[start] == '0';
                const bool point = src[i] == '.' && src[i + 1] >= '0' && src[i + 1] <= '9';
                if (leadZero || digits > 12)
                    spellDigits (dst, n, cap, src + start, digits);
                else
                {
                    unsigned long long value = 0;
                    for (int k = start; k < i; ++k)
                        value = value * 10ull + (unsigned) (src[k] - '0');
                    sayCardinal (dst, n, cap, value);
                }
                if (point)
                {
                    appendPiece (dst, n, cap, "POINT");
                    ++i;
                    const int frac = i;
                    while (src[i] >= '0' && src[i] <= '9' && i - frac < 8)
                        ++i;
                    spellDigits (dst, n, cap, src + frac, i - frac);
                }
                continue;
            }
            if (n < cap)
                dst[n++] = (char) c;
            ++i;
        }
        if (n < cap)
            dst[n] = 0;
        else if (cap > 0)
            dst[cap - 1] = 0;
    }
}

int renderSamSpeech (float* dst, int maxLen, double sampleRate,
                     const char* text, int speed, int pitch, int mouth, int throat,
                     int* cut)
{
    if (cut != nullptr)
        *cut = 0;
    if (dst == nullptr || maxLen < 8 || sampleRate < 1000.0)
        return 0;

    char line[256];
    std::memset (line, 0, sizeof line);
    const char* phrase = (text != nullptr && text[0] != 0) ? text : "ah";
    char spoken[240];
    speakNumbers (phrase, spoken, (int) sizeof spoken);
    const char* src = spoken[0] != 0 ? spoken : "ah";
    int n = 0;
    for (int i = 0; src[i] != 0 && n < 200; ++i)
    {
        unsigned char c = (unsigned char) src[i];
        if (c >= 'a' && c <= 'z')
            c = (unsigned char) (c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || c == ' ' || c == '\'' || c == '.' || c == ',' || c == '?' || c == '!')
            line[n++] = (char) c;
    }
    if (n == 0)
    {
        line[0] = 'A';
        line[1] = 'H';
        n = 2;
    }
    line[n] = '[';

    if (! TextToPhonemes ((unsigned char*) line))
    {
        std::memset (line, 0, sizeof line);
        line[0] = 'A';
        line[1] = 'A';
        line[2] = (char) 0x9b;
    }

    SetInput (line);
    const int speedByte = std::clamp (speed, 1, 255);
    const int pitchByte = std::clamp (pitch, 1, 255);
    const int mouthByte = std::clamp (mouth, 0, 255);
    const int throatByte = std::clamp (throat, 0, 255);
    SamSetSpeed ((unsigned char) speedByte);
    SetPitch ((unsigned char) pitchByte);
    SetMouth ((unsigned char) mouthByte);
    SetThroat ((unsigned char) throatByte);
    EnableSingmode();
    samError = 0;
    samOverflow = 0;
    if (! SAMMain() || samError || samOverflow)
    {
        ReleaseSamBuffer();
        if (cut != nullptr)
            *cut = 1;
        return 0;
    }

    const auto* raw = (const unsigned char*) GetBuffer();
    if (raw == nullptr)
        return 0;
    const int samN = std::max (0, GetBufferLength() / 50);
    const int use = std::min (samN + 4, 22050 * 8 - 1);
    const double step = 22050.0 / sampleRate;
    int outN = (use > 1 && step > 0.0) ? (int) std::floor ((double) (use - 1) / step) : 0;
    outN = std::clamp (outN, 0, maxLen);

    float peak = 0.f;
    for (int i = 0; i < outN; ++i)
    {
        const double pos = (double) i * step;
        const int a = (int) pos;
        const int b = std::min (a + 1, use - 1);
        const float frac = (float) (pos - a);
        const float sa = (float) ((int) raw[a] - 128) / 128.f;
        const float sb = (float) ((int) raw[b] - 128) / 128.f;
        dst[i] = sa * (1.f - frac) + sb * frac;
        peak = std::max (peak, std::abs (dst[i]));
    }
    ReleaseSamBuffer();
    if (cut != nullptr)
        *cut = 0;

    const float gain = peak > 0.0001f ? 0.42f / peak : 0.f;
    for (int i = 0; i < outN; ++i)
        dst[i] *= gain;
    return outN >= 2 ? outN : 0;
}
