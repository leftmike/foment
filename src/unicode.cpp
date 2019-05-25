/*

Foment

*/

#include <stdio.h>
#include <string.h>
#include "foment.hpp"
#include "unicode.hpp"
#include "unicase.hpp"

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

#ifdef FOMENT_DEBUG
static int32_t DigitValueInternal(FCh ch)
#else // FOMENT_DEBUG
int32_t DigitValue(FCh ch)
#endif // FOMENT_DEBUG
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

#ifdef FOMENT_DEBUG
int32_t DigitValue(FCh ch)
{
    int32_t dv = DigitValueInternal(ch);
    if (dv >= 0)
    {
        FAssert(DigitP(ch));
    }
    else
    {
        if (DigitP(ch) != 0)
            printf("DigitP != 0: %d %x\n", ch, ch);
        //FAssert(DigitP(ch) == 0);
    }
    return(dv);
}
#endif // FOMENT_DEBUG

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

