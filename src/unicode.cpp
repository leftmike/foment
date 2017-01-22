/*

Foment

*/

#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "unicode.hpp"
#include "unidata.hpp"

unsigned char Utf8TrailingBytes[256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 4, 4,
    4, 4, 5, 5, 5, 5
};

static const FByte Utf8FirstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};
#define Utf8ByteMask 0xBF
#define Utf8ByteMark 0x80

static const FCh Utf8Offsets[6] =
{
    0x00000000,
    0x00003080,
    0x000E2080,
    0x03C82080,
    0xFA082080,
    0x82082080
};

static ulong_t ChLengthOfUtf8(FByte * b, ulong_t bl)
{
    ulong_t sl = 0;

    for (ulong_t bdx = 0; bdx < bl; sl++)
        bdx += Utf8TrailingBytes[b[bdx]] + 1;

    return(sl);
}

static ulong_t Utf8LengthOfCh(FCh * s, ulong_t sl)
{
    ulong_t bl = 0;

    for (ulong_t sdx = 0; sdx < sl; sdx++)
    {
        if (s[sdx] < 0x80UL)
            bl += 1;
        else if (s[sdx] < 0x800UL)
            bl += 2;
        else if (s[sdx] < 0x10000UL)
            bl += 3;
        else if (s[sdx] <= MaximumUnicodeCharacter)
            bl += 4;
        else
            bl += 3; // ch = UnicodeReplacementCharacter;
    }

    return(bl);
}

FCh ConvertUtf8ToCh(FByte * b, ulong_t bl)
{
    FCh ch = 0;
    ulong_t bdx = 0;
    ulong_t eb = Utf8TrailingBytes[b[bdx]];

    FAssert(bdx + eb < bl);

    switch(eb)
    {
        case 5: ch += b[bdx]; bdx += 1; ch <<= 6;
        case 4: ch += b[bdx]; bdx += 1; ch <<= 6;
        case 3: ch += b[bdx]; bdx += 1; ch <<= 6;
        case 2: ch += b[bdx]; bdx += 1; ch <<= 6;
        case 1: ch += b[bdx]; bdx += 1; ch <<= 6;
        case 0: ch += b[bdx]; bdx += 1;
    }

    ch -= Utf8Offsets[eb];

    if (ch > MaximumUnicodeCharacter
            || (ch >= Utf16HighSurrogateStart && ch <= Utf16LowSurrogateEnd))
        ch = UnicodeReplacementCharacter;

    return(ch);
}

FObject ConvertUtf8ToString(FByte * b, ulong_t bl)
{
    ulong_t sl = ChLengthOfUtf8(b, bl);
    FObject s = MakeString(0, sl);
    ulong_t bdx = 0;

    for (ulong_t sdx = 0; sdx < sl; sdx++)
    {
        FCh ch = 0;
        ulong_t eb = Utf8TrailingBytes[b[bdx]];

        FAssert(bdx + eb < bl);

        switch(eb)
        {
            case 5: ch += b[bdx]; bdx += 1; ch <<= 6;
            case 4: ch += b[bdx]; bdx += 1; ch <<= 6;
            case 3: ch += b[bdx]; bdx += 1; ch <<= 6;
            case 2: ch += b[bdx]; bdx += 1; ch <<= 6;
            case 1: ch += b[bdx]; bdx += 1; ch <<= 6;
            case 0: ch += b[bdx]; bdx += 1;
        }

        ch -= Utf8Offsets[eb];

        if (ch > MaximumUnicodeCharacter
                || (ch >= Utf16HighSurrogateStart && ch <= Utf16LowSurrogateEnd))
            ch = UnicodeReplacementCharacter;

        AsString(s)->String[sdx] = ch;
    }

    return(s);
}

FObject ConvertStringToUtf8(FCh * s, ulong_t sl, long_t ztf)
{
    ulong_t bl = Utf8LengthOfCh(s, sl);
    FObject b = MakeBytevector(bl + (ztf ? 1 : 0));
    ulong_t bdx = 0;

    for (ulong_t sdx = 0; sdx < sl; sdx++)
    {
        FCh ch = s[sdx];
        ulong_t bw;

        if (ch < 0x80UL)
            bw = 1;
        else if (ch < 0x800UL)
            bw = 2;
        else if (ch < 0x10000UL)
            bw = 3;
        else if (ch <= MaximumUnicodeCharacter)
            bw = 4;
        else
        {
            ch = UnicodeReplacementCharacter;
            bw = 3;
        }

        FAssert(bdx + bw <= bl);

        bdx += bw;
        switch (bw)
        {
            case 4:
                bdx -= 1;
                AsBytevector(b)->Vector[bdx] = (FByte) ((ch | Utf8ByteMark) & Utf8ByteMask);
                ch >>= 6;

            case 3:
                bdx -= 1;
                AsBytevector(b)->Vector[bdx] = (FByte) ((ch | Utf8ByteMark) & Utf8ByteMask);
                ch >>= 6;

            case 2:
                bdx -= 1;
                AsBytevector(b)->Vector[bdx] = (FByte) ((ch | Utf8ByteMark) & Utf8ByteMask);
                ch >>= 6;

            case 1:
                bdx -= 1;
                AsBytevector(b)->Vector[bdx] = (FByte) (ch | Utf8FirstByteMark[bw]);
        }
        bdx += bw;
    }

    if (ztf)
        AsBytevector(b)->Vector[bl] = 0;
    return(b);
}

static ulong_t Utf16LengthOfCh(FCh * s, ulong_t sl)
{
    ulong_t ssl = 0;

    for (ulong_t idx = 0; idx < sl; idx++)
    {
        ssl += 1;

        if (s[idx] > 0xFFFF && s[idx] <= 0x10FFFF)
            ssl += 1;
    }

    return(ssl);
}

static ulong_t ChLengthOfUtf16(FCh16 * ss, ulong_t ssl)
{
    ulong_t sl = 0;

    while (ssl > 0)
    {
        if (*ss < Utf16HighSurrogateStart || *ss > Utf16HighSurrogateEnd)
            sl += 1;

        ss += 1;
        ssl -= 1;
    }

    return(sl);
}

FObject ConvertUtf16ToString(FCh16 * b, ulong_t bl)
{
    ulong_t sl = ChLengthOfUtf16(b, bl);
    FObject s = MakeString(0, sl);
    ulong_t bdx = 0;

    for (ulong_t sdx = 0; sdx < sl; sdx++)
    {
        FAssert(bdx < bl);

        FCh ch = b[bdx];

        if (ch >= Utf16HighSurrogateStart && ch <= Utf16HighSurrogateEnd)
        {
            bdx += 1;

            FAssert(bdx < bl);

            if (b[bdx] >= Utf16LowSurrogateStart && b[bdx] <= Utf16LowSurrogateEnd)
                ch = ((ch - Utf16HighSurrogateStart) << Utf16HalfShift)
                        + (((FCh) b[bdx]) - Utf16LowSurrogateStart) + Utf16HalfBase;
            else
                ch = UnicodeReplacementCharacter;
        }
        else if (ch >= Utf16LowSurrogateStart && ch <= Utf16LowSurrogateEnd)
            ch = UnicodeReplacementCharacter;

        AsString(s)->String[sdx] = ch;

        bdx += 1;
    }

    return(s);
}

FObject ConvertStringToUtf16(FCh * s, ulong_t sl, long_t ztf, ulong_t el)
{
    ulong_t ul = Utf16LengthOfCh(s, sl);
    FObject b = MakeBytevector((ul + (ztf ? 1 : 0) + el) * sizeof(FCh16));
    FCh16 * u = (FCh16 *) AsBytevector(b)->Vector;
    ulong_t udx = 0;

    for (ulong_t sdx = 0; sdx < sl; sdx++)
    {
        FAssert(udx < ul);

        FCh ch = s[sdx];

        if (ch <= 0xFFFF)
        {
            if (ch >= Utf16HighSurrogateStart && ch <= Utf16LowSurrogateEnd)
                ch = UnicodeReplacementCharacter;

            u[udx] = (FCh16) ch;
        }
        else if (ch > MaximumUnicodeCharacter)
            u[udx] = UnicodeReplacementCharacter;
        else
        {
            FAssert(udx + 1 < ul);

            ch -= Utf16HalfBase;
            u[udx] = (FCh16) ((ch >> Utf16HalfShift) + Utf16HighSurrogateStart);
            udx += 1;
            u[udx] = (FCh16) ((ch & Utf16HalfMask) + Utf16LowSurrogateStart);
        }

        udx += 1;
    }

    if (ztf)
        u[ul] = 0;
    return(b);
}

#ifdef FOMENT_WINDOWS
FObject MakeStringS(FChS * ss)
{
    return(ConvertUtf16ToString(ss, wcslen(ss)));
}

FObject MakeStringS(FChS * ss, ulong_t ssl)
{
    return(ConvertUtf16ToString(ss, ssl));
}
#endif // FOMENT_WINDOWS

#ifdef FOMENT_UNIX
FObject MakeStringS(FChS * ss)
{
    return(ConvertUtf8ToString((FByte *) ss, strlen(ss)));
}

FObject MakeStringS(FChS * ss, ulong_t ssl)
{
    return(ConvertUtf8ToString((FByte *) ss, ssl));
}
#endif // FOMENT_UNIX

int WhitespaceP(FCh ch)
{
    // From:
    // PropList-6.2.0.txt
    // Date: 2012-05-23, 20:34:59 GMT [MD]

    if (ch >= 0x0009 && ch <= 0x000D)
        return(1);
    else if (ch == 0x0020)
        return(1);
    else if (ch == 0x0085)
        return(1);
    else if (ch == 0x00A0)
        return(1);
    else if (ch == 0x1680)
        return(1);
    else if (ch == 0x180E)
        return(1);
    else if (ch >= 0x2000 && ch <= 0x200A)
        return(1);
    else if (ch == 0x2028)
        return(1);
    else if (ch == 0x2029)
        return(1);
    else if (ch == 0x202F)
        return(1);
    else if (ch == 0x205F)
        return(1);
    else if (ch == 0x3000)
        return(1);

    return(0);
}

int32_t DigitValue(FCh ch)
{
    // From:
    // DerivedNumericType-6.2.0.txt
    // Date: 2012-08-13, 19:20:20 GMT [MD]

    // Return 0 to 9 for digit values and -1 if not a digit.

    if (ch < 0x1D00)
    {
        switch (ch >> 8)
        {
        case 0x00:
            if (ch >= 0x0030 && ch <= 0x0039)
                return(ch - 0x0030);
            return(-1);

        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
            return(-1);

        case 0x06:
            if (ch >= 0x0660 && ch <= 0x0669)
                return(ch - 0x0660);
            if (ch >= 0x06F0 && ch <= 0x06F9)
                return(ch - 0x06F0);
            return(-1);

        case 0x07:
            if (ch >= 0x07C0 && ch <= 0x07C9)
                return(ch - 0x07C0);
            return(-1);

        case 0x08:
            return(-1);

        case 0x09:
            if (ch >= 0x0966 && ch <= 0x096F)
                return(ch - 0x0966);
            if (ch >= 0x09E6 && ch <= 0x09EF)
                return(ch - 0x09E6);
            return(-1);

        case 0x0A:
            if (ch >= 0x0A66 && ch <= 0x0A6F)
                return(ch - 0x0A66);
            if (ch >= 0x0AE6 && ch <= 0x0AEF)
                return(ch - 0x0AE6);
            return(-1);

        case 0x0B:
            if (ch >= 0x0B66 && ch <= 0x0B6F)
                return(ch - 0x0B66);
            if (ch >= 0x0BE6 && ch <= 0x0BEF)
                return(ch - 0x0BE6);
            return(-1);

        case 0x0C:
            if (ch >= 0x0C66 && ch <= 0x0C6F)
                return(ch - 0x0C66);
            if (ch >= 0x0CE6 && ch <= 0x0CEF)
                return(ch - 0x0CE6);
            return(-1);

        case 0x0D:
            if (ch >= 0x0D66 && ch <= 0x0D6F)
                return(ch - 0x0D66);
            return(-1);

        case 0x0E:
            if (ch >= 0x0E50 && ch <= 0x0E59)
                return(ch - 0x0E50);
            if (ch >= 0x0ED0 && ch <= 0x0ED9)
                return(ch - 0x0ED0);
            return(-1);

        case 0x0F:
            if (ch >= 0x0F20 && ch <= 0x0F29)
                return(ch - 0x0F20);
            return(-1);

        case 0x10:
            if (ch >= 0x1040 && ch <= 0x1049)
                return(ch - 0x1040);
            if (ch >= 0x1090 && ch <= 0x1099)
                return(ch - 0x1090);
            return(-1);

        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
            return(-1);

        case 0x17:
            if (ch >= 0x17E0 && ch <= 0x17E9)
                return(ch - 0x17E0);
            return(-1);

        case 0x18:
            if (ch >= 0x1810 && ch <= 0x1819)
                return(ch - 0x1810);
            return(-1);

        case 0x19:
            if (ch >= 0x1946 && ch <= 0x194F)
                return(ch - 0x1946);
            if (ch >= 0x19D0 && ch <= 0x19D9)
                return(ch - 0x19D0);
            return(-1);

        case 0x1A:
            if (ch >= 0x1A80 && ch <= 0x1A89)
                return(ch - 0x1A80);
            if (ch >= 0x1A90 && ch <= 0x1A99)
                return(ch - 0x1A90);
            return(-1);

        case 0x1B:
            if (ch >= 0x1B50 && ch <= 0x1B59)
                return(ch - 0x1B50);
            if (ch >= 0x1BB0 && ch <= 0x1BB9)
                return(ch - 0x1BB0);
            return(-1);

        case 0x1C:
            if (ch >= 0x1C40 && ch <= 0x1C49)
                return(ch - 0x1C40);
            if (ch >= 0x1C50 && ch <= 0x1C59)
                return(ch - 0x1C50);
            return(-1);
        }
    }

    if (ch < 0xA620)
        return(-1);

    if (ch < 0xAC00)
    {
        switch (ch >> 8)
        {
        case 0xA6:
            if (ch >= 0xA620 && ch <= 0xA629)
                return(ch - 0xA620);
            return(-1);

        case 0xA7:
            return(-1);

        case 0xA8:
            if (ch >= 0xA8D0 && ch <= 0xA8D9)
                return(ch - 0xA8D0);
            return(-1);

        case 0xA9:
            if (ch >= 0xA900 && ch <= 0xA909)
                return(ch - 0xA900);
            if (ch >= 0xA9D0 && ch <= 0xA9D9)
                return(ch - 0xA9D0);
            return(-1);

        case 0xAA:
            if (ch >= 0xAA50 && ch <= 0xAA59)
                return(ch - 0xAA50);
            return(-1);

        case 0xAB:
            if (ch >= 0xABF0 && ch <= 0xABF9)
                return(ch - 0xABF0);
            return(-1);
        }
    }

    if (ch >= 0xFF10 && ch <= 0xFF19)
        return(ch - 0xFF10);

    if (ch >= 0x104A0 && ch <= 0x104A9)
        return(ch - 0x104A0);

    if (ch >= 0x11066 && ch <= 0x1106F)
        return(ch - 0x11066);

    if (ch >= 0x110F0 && ch <= 0x110F9)
        return(ch - 0x110F0);

    if (ch >= 0x11136 && ch <= 0x1113F)
        return(ch - 0x11136);

    if (ch >= 0x111D0 && ch <= 0x111D9)
        return(ch - 0x111D0);

    if (ch >= 0x116C0 && ch <= 0x116C9)
        return(ch - 0x116C0);

    if (ch >= 0x1D7CE && ch <= 0x1D7FF)
        return(ch - 0x1D7CE);

    return(-1);
}

static inline unsigned int MapCh(const unsigned int * mp, FCh bs, FCh ch)
{
    FAssert(ch >= bs);

    ch -= bs;
    return(mp[ch / 32] & (1 << (ch % 32)));
}

unsigned int AlphabeticP(FCh ch)
{
    // From:
    // DerivedCoreProperties-6.2.0.txt
    // Date: 2012-05-20, 00:42:31 GMT [MD]

    switch (ch >> 12)
    {
    case 0x00:
    case 0x01:
        return(MapCh(Alphabetic0x0000To0x1fff, 0, ch));

    case 0x02:
        if (ch >= 0x2060 && ch <= 0x219F)
            return(MapCh(Alphabetic0x2060To0x219f, 0x2060, ch));
        if (ch >= 0x24A0 && ch <= 0x24FF)
            return(MapCh(Alphabetic0x24a0To0x24ff, 0x24A0, ch));
        if (ch >= 0x2C00 && ch <= 0x2E3F)
            return(MapCh(Alphabetic0x2c00To0x2e3f, 0x2C00, ch));
        break;

    case 0x03:
        if (ch >= 0x3000 && ch <= 0x31FF)
            return(MapCh(Alphabetic0x3000To0x31ff, 0x3000, ch));

        // No break.

    case 0x04:
        if (ch >= 0x3400 && ch <= 0x4DB5)
            return(1);

        // No break.

    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
        if (ch >= 0x4E00 && ch <= 0x9FCC)
            return(1);
        break;

    case 0x0A:
        if (ch >= 0xA000 && ch <= 0xA48C)
            return(1);
        if (ch >= 0xA480 && ch <= 0xABFF)
            return(MapCh(Alphabetic0xa480To0xabff, 0xA480, ch));

        // No break.

    case 0x0B:
    case 0x0C:
    case 0x0D:
        if (ch >= 0xAC00 && ch <= 0xD7A3)
            return(1);
        if (ch >= 0xD7A0 && ch <= 0xD7FF)
            return(MapCh(Alphabetic0xd7a0To0xd7ff, 0xD7A0, ch));
        break;

    case 0x0E:
        break;

    case 0x0F:
    case 0x10:
        if (ch >= 0xF900 && ch <= 0x1049F)
            return(MapCh(Alphabetic0xf900To0x1049f, 0xF900, ch));
        if (ch >= 0x10800 && ch <=0x10C5F)
            return(MapCh(Alphabetic0x10800To0x10c5f, 0x10800, ch));
        break;

    case 0x11:
        if (ch >= 0x11000 && ch <= 0x111DF)
            return(MapCh(Alphabetic0x11000To0x111df, 0x11000, ch));
        if (ch >= 11680 && ch <= 0x116AF)
            return(1);
        break;

    case 0x12:
        if (ch >= 0x12000 && ch <= 0x1236E)
            return(1);
        if (ch >= 0x12400 && ch <= 0x12462)
            return(1);
        break;

    case 0x13:
        if (ch >= 0x13000 && ch <= 0x1342E)
            return(1);

    case 0x14:
    case 0x15:
        break;

    case 0x16:
        if (ch >= 0x16800 && ch <= 0x16A38)
            return(1);
        if (ch >= 0x16F00 && ch <= 0x16F9F)
            return(MapCh(Alphabetic0x16f00To0x16f9f, 0x16F00, ch));
        break;

    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
        break;

    case 0x1B:
        if (ch >= 0x1B000 && ch <= 0x1B001)
            return(1);
        break;

    case 0x1C:
        break;

    case 0x1D:
        if (ch >= 0x1D400 && ch <= 0x1D7DF)
            return(MapCh(Alphabetic0x1d400To0x1d7df, 0x1D400, ch));
        if (ch >= 0x1EE00 && ch <= 0x1EEBF)
            return(MapCh(Alphabetic0x1ee00To0x1eebf, 0x1EE00, ch));
        break;

    case 0x1E:
    case 0x1F:
        break;

    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
        if (ch >= 0x20000 && ch <= 0x2A6D6)
            return(1);

        // No break.

    case 0x2B:
        if (ch >= 0x2A700 && ch <= 0x2B734)
            return(1);
        if (ch >= 0x2B740 && ch <= 0x2B81D)
            return(1);
        break;

    case 0x2C:
    case 0x2D:
    case 0x2E:
        break;

    case 0x2F:
        if (ch >= 0x2F800 && ch <= 0x2FA1D)
            return(1);
        break;
    }

    return(0);
}

unsigned int UppercaseP(FCh ch)
{
    // From:
    // DerivedCoreProperties-6.2.0.txt
    // Date: 2012-05-20, 00:42:31 GMT [MD]

    switch (ch >> 12)
    {
    case 0x00:
        if (ch <= 0x059F)
            return(MapCh(Uppercase0x0000To0x059f, 0, ch));
        break;

    case 0x01:
        if (ch >= 0x10A0 && ch <= 0x10DF)
            return(MapCh(Uppercase0x10a0To0x10df, 0x10A0, ch));
        if (ch >= 0x1E00 && ch <= 0x1FFF)
            return(MapCh(Uppercase0x1e00To0x1fff, 0x1E00, ch));
        break;

    case 0x02:
        if (ch >= 0x2100 && ch <= 0x219F)
            return(MapCh(Uppercase0x2100To0x219f, 0x2100, ch));
        if (ch >= 0x24B6 && ch <= 0x24CF)
            return(1);
        if (ch >= 0x2C00 && ch <= 0x2D3F)
            return(MapCh(Uppercase0x2c00To0x2d3f, 0x2C00, ch));
        break;

    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
        break;

    case 0x0A:
        if (ch >= 0xA640 && ch <= 0xA7BF)
            return(MapCh(Uppercase0xa640To0xa7bf, 0xA640, ch));
        break;

    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x0E:
        break;

    case 0x0F:
        if (ch >= 0xFF21 && ch <= 0xFF3A)
            return(1);
        break;

    case 0x10:
        if (ch >= 0x10400 && ch <= 0x10427)
            return(1);
        break;

    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
        break;

    case 0x1D:
        if (ch >= 0x1D400 && ch <= 0x1D7DF)
            return(MapCh(Uppercase0x1d400To0x1d7df, 0x1D400, ch));
        break;
    }

    return(0);
}

unsigned int LowercaseP(FCh ch)
{
    // From:
    // DerivedCoreProperties-6.2.0.txt
    // Date: 2012-05-20, 00:42:31 GMT [MD]

    switch (ch >> 12)
    {
    case 0x00:
        if (ch <= 0x059F)
            return(MapCh(Lowercase0x0000To0x059f, 0, ch));
        break;

    case 0x01:
        if (ch >= 0x1D00 && ch <= 0x1FFF)
            return(MapCh(Lowercase0x1d00To0x1fff, 0x1D00, ch));
        break;

    case 0x02:
        if (ch >= 0x2060 && ch <= 0x219F)
            return(MapCh(Lowercase0x2060To0x219f, 0x2060, ch));
        if (ch >= 0x24D0 && ch <= 0x24E9)
            return(1);
        if (ch >= 0x2C20 && ch <= 0x2D3F)
            return(MapCh(Lowercase0x2c20To0x2d3f, 0x2C20, ch));
        break;

    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
        break;

    case 0x0A:
        if (ch >= 0xA640 && ch <= 0xA7FF)
            return(MapCh(Lowercase0xa640To0xa7ff, 0xA640, ch));
        break;

    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x0E:
        break;

    case 0x0F:
        if (ch >= 0xFB00 && ch <= 0xFB06)
            return(1);
        if (ch >= 0xFB13 && ch <= 0xFB17)
            return(1);
        if (ch >= 0xFF41 && ch <= 0xFF5A)
            return(1);
        break;

    case 0x10:
        if (ch >= 0x10428 && ch <= 0x1044F)
            return(1);
        break;

    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
        break;

    case 0x1D:
        if (ch >= 0x1D400 && ch <= 0x1D7DF)
            return(MapCh(Lowercase0x1d400To0x1d7df, 0x1D400, ch));
    }

    return(0);
}
unsigned int CharFullfoldLength(FCh ch)
{
    // From:
    // CaseFolding-6.2.0.txt
    // Date: 2012-08-14, 17:54:49 GMT [MD]

    if (ch < 0x2000)
    {
        if (FullfoldSet[ch / 32] & (1 << (ch % 32)))
        {
            if (ch == 0x00df)
                return(Fullfold0x00dfTo0x00df[0].Count);
            else if (ch >= 0x0130 && ch <= 0x0149)
                return(Fullfold0x0130To0x0149[ch - 0x0130].Count);
            else if (ch == 0x01f0)
                return(Fullfold0x01f0To0x01f0[0].Count);
            else if (ch >= 0x0390 && ch <= 0x03b0)
                return(Fullfold0x0390To0x03b0[ch - 0x0390].Count);
            else if (ch == 0x0587)
                return(Fullfold0x0587To0x0587[0].Count);
            else if (ch >= 0x1e96 && ch <= 0x1e9e)
                return(Fullfold0x1e96To0x1e9e[ch - 0x1e96].Count);
            else if (ch >= 0x1f50 && ch <= 0x1f56)
                return(Fullfold0x1f50To0x1f56[ch - 0x1f50].Count);
            else if (ch >= 0x1f80 && ch <= 0x1ffc)
                return(Fullfold0x1f80To0x1ffc[ch - 0x1f80].Count);
        }

        return(1);
    }
    else if (ch >= 0xfb00 && ch <= 0xfb17)
        return(Fullfold0xfb00To0xfb17[ch - 0xfb00].Count);

    return(1);
}

FCh * CharFullfold(FCh ch)
{
    // From:
    // CaseFolding-6.2.0.txt
    // Date: 2012-08-14, 17:54:49 GMT [MD]

    FAssert(ch >= 0x2000 || FullfoldSet[ch / 32] & (1 << (ch % 32)));

    if (ch == 0x00df)
        return(Fullfold0x00dfTo0x00df[0].Chars);
    else if (ch >= 0x0130 && ch <= 0x0149)
        return(Fullfold0x0130To0x0149[ch - 0x0130].Chars);
    else if (ch == 0x01f0)
        return(Fullfold0x01f0To0x01f0[0].Chars);
    else if (ch >= 0x0390 && ch <= 0x03b0)
        return(Fullfold0x0390To0x03b0[ch - 0x0390].Chars);
    else if (ch == 0x0587)
        return(Fullfold0x0587To0x0587[0].Chars);
    else if (ch >= 0x1e96 && ch <= 0x1e9e)
        return(Fullfold0x1e96To0x1e9e[ch - 0x1e96].Chars);
    else if (ch >= 0x1f50 && ch <= 0x1f56)
        return(Fullfold0x1f50To0x1f56[ch - 0x1f50].Chars);
    else if (ch >= 0x1f80 && ch <= 0x1ffc)
        return(Fullfold0x1f80To0x1ffc[ch - 0x1f80].Chars);

    FAssert(ch >= 0xfb00 && ch <= 0xfb17);

    return(Fullfold0xfb00To0xfb17[ch - 0xfb00].Chars);
}

unsigned int CharFullupLength(FCh ch)
{
    // From:
    // SpecialCasing-6.2.0.txt
    // Date: 2012-05-23, 20:35:15 GMT [MD]

    if (ch < 0x2000)
    {
        if (FullupSet[ch / 32] & (1 << (ch % 32)))
        {
            if (ch == 0x00df)
                return(Fullup0x00dfTo0x00df[0].Count);
            else if (ch == 0x0149)
                return(Fullup0x0149To0x0149[0].Count);
            else if (ch == 0x01f0)
                return(Fullup0x01f0To0x01f0[0].Count);
            else if (ch >= 0x0390 && ch <= 0x03b0)
                return(Fullup0x0390To0x03b0[ch - 0x0390].Count);
            else if (ch == 0x0587)
                return(Fullup0x0587To0x0587[0].Count);
            else if (ch >= 0x1e96 && ch <= 0x1e9a)
                return(Fullup0x1e96To0x1e9a[ch - 0x1e96].Count);
            else if (ch >= 0x1f50 && ch <= 0x1f56)
                return(Fullup0x1f50To0x1f56[ch - 0x1f50].Count);
            else if (ch >= 0x1f80 && ch <= 0x1ffc)
                return(Fullup0x1f80To0x1ffc[ch - 0x1f80].Count);
        }

        return(1);
    }
    else if (ch >= 0xfb00 && ch <= 0xfb17)
        return(Fullup0xfb00To0xfb17[ch - 0xfb00].Count);

    return(1);
}

FCh * CharFullup(FCh ch)
{
    // From:
    // SpecialCasing-6.2.0.txt
    // Date: 2012-05-23, 20:35:15 GMT [MD]

    FAssert(ch >= 0x2000 || FullupSet[ch / 32] & (1 << (ch % 32)));

    if (ch == 0x00df)
        return(Fullup0x00dfTo0x00df[0].Chars);
    else if (ch == 0x0149)
        return(Fullup0x0149To0x0149[0].Chars);
    else if (ch == 0x01f0)
        return(Fullup0x01f0To0x01f0[0].Chars);
    else if (ch >= 0x0390 && ch <= 0x03b0)
        return(Fullup0x0390To0x03b0[ch - 0x0390].Chars);
    else if (ch == 0x0587)
        return(Fullup0x0587To0x0587[0].Chars);
    else if (ch >= 0x1e96 && ch <= 0x1e9a)
        return(Fullup0x1e96To0x1e9a[ch - 0x1e96].Chars);
    else if (ch >= 0x1f50 && ch <= 0x1f56)
        return(Fullup0x1f50To0x1f56[ch - 0x1f50].Chars);
    else if (ch >= 0x1f80 && ch <= 0x1ffc)
        return(Fullup0x1f80To0x1ffc[ch - 0x1f80].Chars);

    FAssert(ch >= 0xfb00 && ch <= 0xfb17);

    return(Fullup0xfb00To0xfb17[ch - 0xfb00].Chars);
}

unsigned int CharFulldownLength(FCh ch)
{
    // From:
    // SpecialCasing-6.2.0.txt
    // Date: 2012-05-23, 20:35:15 GMT [MD]

    if (ch == 0x0130)
        return(2);
    return(1);
}

static FCh Fulldown0x0130[] = {0x0069, 0x0307};

FCh * CharFulldown(FCh ch)
{
    // From:
    // SpecialCasing-6.2.0.txt
    // Date: 2012-05-23, 20:35:15 GMT [MD]

    FAssert(ch == 0x0130);

    return(Fulldown0x0130);
}

