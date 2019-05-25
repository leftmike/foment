/*

Foment

*/

#include "foment.hpp"

#if !defined(FOMENT_BSD) && !defined(FOMENT_OSX)
#include <malloc.h>
#endif // FOMENT_BSD
#include <stdlib.h>
#include <string.h>
#include "unicode.hpp"
#include "unicrng.hpp"

// ---- Roots ----

static FObject EmptyCharSet = NoValueObject;
static FObject FullCharSet = NoValueObject;

// ---- CharSets ----

typedef struct
{
    FCharRange Ranges[1];
} FCharSet;

#define AsCharSet(obj) ((FCharSet *) (obj))
#define CharSetP(obj) (IndirectTag(obj) == CharSetTag)

static FObject MakeCharSet(ulong_t nr, FCharRange * r, const char * who)
{
    FCharSet * cset = (FCharSet *) MakeObject(CharSetTag, sizeof(FCharRange) * nr, 0, who);
    memcpy(cset->Ranges, r, sizeof(FCharRange) * nr);
    return(cset);
}

inline ulong_t NumRanges(FObject obj)
{
    FAssert(CharSetP(obj));
    FAssert(ByteLength(obj) % sizeof(FCharRange) == 0);

    return(ByteLength(obj) / sizeof(FCharRange));
}

inline int InvalidUCSCh(FCh ch)
{
    if ((ch > 0xD7FF && ch < 0xE000) || ch > 0x10FFFF)
        return(1);
    return(0);
}

#ifdef FOMENT_DEBUG
static int ValidCharSet(FObject obj)
{
    if (CharSetP(obj) == 0)
        return(0);

    ulong_t nr = NumRanges(obj);
    FCharSet * cset = AsCharSet(obj);
    for (ulong_t rdx = 0; rdx < nr; rdx++)
    {
        if (cset->Ranges[rdx].Start > cset->Ranges[rdx].End)
            return(0);
        if (rdx > 0 && cset->Ranges[rdx].Start <= cset->Ranges[rdx - 1].End)
            return(0);
    }

    return(1);
}
#endif // FOMENT_DEBUG

static int RangeCmp(const void * key, const void * datum)
{
    FCh ch = *((const FCh *) key);
    const FCharRange * r = (const FCharRange *) datum;

    if (ch < r->Start)
        return(-1);
    else if (ch > r->End)
        return(1);
    return(0);
}

static int CharRangeMemberP(ulong_t nr, FCharRange * r, FCh ch)
{
    if (nr > 10)
        return(bsearch(&ch, r, nr, sizeof(FCharRange), RangeCmp) != 0);
    else
    {
        for (ulong_t ndx = 0; ndx < nr; ndx++)
        {
            if (ch >= r[ndx].Start)
            {
                if (ch <= r[ndx].End)
                    return(1);
            }
            else // (ch < r[ndx].Start
                return(0);
        }
    }

    return(0);
}

int WhitespaceP(FCh ch)
{
    return(CharRangeMemberP(sizeof(WhitespaceCharRange) / sizeof(FCharRange),
            WhitespaceCharRange, ch));
}

unsigned int DigitP(FCh ch)
{
    return(CharRangeMemberP(sizeof(DigitCharRange) / sizeof(FCharRange), DigitCharRange, ch));
}

unsigned int AlphabeticP(FCh ch)
{
    return(CharRangeMemberP(sizeof(LetterCharRange) / sizeof(FCharRange), LetterCharRange, ch));
}

unsigned int UppercaseP(FCh ch)
{
    return(CharRangeMemberP(sizeof(UpperCaseCharRange) / sizeof(FCharRange),
            UpperCaseCharRange, ch));
}

unsigned int LowercaseP(FCh ch)
{
    return(CharRangeMemberP(sizeof(LowerCaseCharRange) / sizeof(FCharRange),
            LowerCaseCharRange, ch));
}

inline int CharSetMemberP(FObject cset, FCh ch)
{
    return(CharRangeMemberP(NumRanges(cset), AsCharSet(cset)->Ranges, ch));
}

inline void CharSetArgCheck(const char * who, FObject obj)
{
    if (CharSetP(obj) == 0)
        RaiseExceptionC(Assertion, who, "expected a character set", List(obj));
}

void WriteCharSet(FWriteContext * wctx, FObject obj)
{
    FCh s[16];
    long_t sl = FixnumAsString((long_t) obj, s, 16);

    wctx->WriteStringC("#<char-set: ");
    wctx->WriteString(s, sl);
    wctx->WriteCh('>');
}

// Test if cset1 is a subset of cset2.
static int CharSetSubset(FObject cset1, FObject cset2)
{
    FAssert(ValidCharSet(cset1));
    FAssert(ValidCharSet(cset2));

    ulong_t nr1 = NumRanges(cset1);

    if (nr1 == 0)
        return(1); // empty char set is a subset of everything

    FCharRange * r1 = AsCharSet(cset1)->Ranges;
    FCharRange * r2 = AsCharSet(cset2)->Ranges;
    ulong_t nr2 = NumRanges(cset2);
    ulong_t ndx2 = 0;
    for (ulong_t ndx1 = 0; ndx1 < nr1; ndx1++)
    {
        while (r1[ndx1].Start > r2[ndx2].End)
        {
            ndx2 += 1;
            if (ndx2 == nr2)
                return(0);
        }

        if (r1[ndx1].Start < r2[ndx2].Start || r1[ndx1].End > r2[ndx2].End)
            return(0);
    }

    return(1);
}

static FObject CharSetUnion(FObject cset1, FObject cset2, const char * who)
{
    FAssert(ValidCharSet(cset1));
    FAssert(ValidCharSet(cset2));

    ulong_t nr1 = NumRanges(cset1);
    ulong_t nr2 = NumRanges(cset2);

    if (nr1 == 0)
        return(cset2);
    if (nr2 == 0)
        return(cset1);

    FCharRange * r = (FCharRange *) malloc(sizeof(FCharRange) * (nr1 + nr2));
    if (r == 0)
        RaiseExceptionC(Assertion, who, "out of memory", EmptyListObject);

    FCharRange * r1 = AsCharSet(cset1)->Ranges;
    FCharRange * r2 = AsCharSet(cset2)->Ranges;
    ulong_t ndx1 = 0;
    ulong_t ndx2 = 0;
    ulong_t rl = 0;

    if (r1[0].Start < r2[0].Start)
    {
        r[0] = r1[0];
        ndx1 = 1;
    }
    else
    {
        r[0] = r2[0];
        ndx2 = 1;
    }

    while (ndx1 < nr1 || ndx2 < nr2)
    {
        FCharRange chr;

        if (ndx1 == nr1)
        {
            chr = r2[ndx2];
            ndx2 += 1;
        }
        else if (ndx2 == nr2)
        {
            chr = r1[ndx1];
            ndx1 += 1;
        }
        else if (r1[ndx1].Start < r2[ndx2].Start)
        {
            chr = r1[ndx1];
            ndx1 += 1;
        }
        else
        {
            chr = r2[ndx2];
            ndx2 += 1;
        }

        if (chr.Start <= r[rl].End)
        {
            if (chr.End > r[rl].End)
                r[rl].End = chr.End;
        }
        else if (chr.Start > r[rl].End)
        {
            rl += 1;
            r[rl] = chr;
        }
    }

    FObject ret = MakeCharSet(rl + 1, r, who);
    free(r);

    FAssert(ValidCharSet(ret));

    return(ret);
}

Define("char-set?", CharSetPPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("char-set?", argc);

    return(CharSetP(argv[0]) ? TrueObject : FalseObject);
}

Define("char-set=", CharSetEqualPrimitive)(long_t argc, FObject argv[])
{
    if (argc == 0)
        return(TrueObject);

    CharSetArgCheck("char-set=", argv[0]);

    ulong_t nr = NumRanges(argv[0]);
    for (long_t adx = 1; adx < argc; adx++)
    {
        CharSetArgCheck("char-set=", argv[adx]);

        if (nr != NumRanges(argv[adx])
            || memcmp(AsCharSet(argv[0])->Ranges, AsCharSet(argv[adx])->Ranges,
                    nr * sizeof(FCharRange)) != 0)
            return(FalseObject);
    }

    return(TrueObject);
}

Define("char-set<=", CharSetSubsetPrimitive)(long_t argc, FObject argv[])
{
    if (argc == 0)
        return(TrueObject);

    CharSetArgCheck("char-set<=", argv[0]);

    for (long_t adx = 1; adx < argc; adx++)
    {
        CharSetArgCheck("char-set<=", argv[adx]);

        if (CharSetSubset(argv[adx - 1], argv[adx]) == 0)
            return(FalseObject);
    }

    return(TrueObject);
}

static uint32_t CharSetHash(FCharSet * cset)
{
    ulong_t sl = NumRanges(cset) * 2;
    FCh * s = (FCh *) cset->Ranges;
    uint32_t h = 0;

    for (; sl > 0; s++, sl--)
        h = ((h << 5) + h) + *s;

    return(NormalizeHash(h));
}

Define("char-set-hash", CharSetHashPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("char-set-hash", argc);
    CharSetArgCheck("char-set-hash", argv[0]);

    long_t bound = MAXIMUM_FIXNUM;
    if (argc == 2)
    {
        FixnumArgCheck("char-set-hash", argv[1]);
        if (AsFixnum(argv[1]) > 0)
            bound = AsFixnum(argv[1]);
    }

    return(MakeFixnum(CharSetHash(AsCharSet(argv[0])) % bound));
}

Define("char-set-cursor", CharSetCursorPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("char-set-cursor", argc);
    CharSetArgCheck("char-set-cursor", argv[0]);

    if (NumRanges(argv[0]) == 0)
        return(FalseObject);

    return(MakeCharacter(AsCharSet(argv[0])->Ranges[0].Start));
}

Define("char-set-cursor-next", CharSetCursorNextPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("char-set-cursor-next", argc);
    CharSetArgCheck("char-set-cursor-next", argv[0]);
    CharacterArgCheck("char-set-cursor-next", argv[1]);

    ulong_t nr = NumRanges(argv[0]);
    FCharRange * r = AsCharSet(argv[0])->Ranges;
    FCh ch = AsCharacter(argv[1]);
    ulong_t ndx;
    for (ndx = 0; ndx < nr; ndx++)
    {
        if (ch >= r[ndx].Start && ch <= r[ndx].End)
        {
            if (ch < r[ndx].End)
                return(MakeCharacter(ch + 1));
            if (ndx + 1 < nr)
                return(MakeCharacter(r[ndx + 1].Start));
            break;
        }
    }

    return(FalseObject);
}

static int ChCmp(const void * p1, const void * p2)
{
    FCh ch1 = *((const FCh *) p1);
    FCh ch2 = *((const FCh *) p2);

    if (ch1 == ch2)
        return(0);
    if (ch1 < ch2)
        return(-1);
    return(1);
}

// WARNING: s will be modified
static FObject StringToCharSet(ulong_t sl, FCh * s, const char * who)
{
    if (sl == 0)
        return(EmptyCharSet);

    FCharRange * r = (FCharRange *) malloc(sizeof(FCharRange) * sl);
    if (r == 0)
        RaiseExceptionC(Assertion, who, "out of memory", EmptyListObject);

    qsort(s, sl, sizeof(FCh), ChCmp);

    r[0].Start = s[0];
    r[0].End = s[0];

    ulong_t rl = 0;
    for (ulong_t sdx = 1; sdx < sl; sdx++)
    {
        if (s[sdx] == r[rl].End + 1)
            r[rl].End += 1;
        else if (s[sdx] > r[rl].End)
        {
            rl += 1;
            r[rl].Start = s[sdx];
            r[rl].End = s[sdx];
        }
    }

    FObject ret = MakeCharSet(rl + 1, r, who);
    free(r);

    FAssert(ValidCharSet(ret));

    return(ret);
}

Define("list->char-set", ListToCharSetPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("list->char-set", argc);

    if (argc == 2)
        CharSetArgCheck("list->char-set", argv[1]);

    ulong_t sl = 0;
    FObject lst = argv[0];
    while (PairP(lst))
    {
        CharacterArgCheck("list->char-set", First(lst));

        sl += 1;
        lst = Rest(lst);
    }

    if (lst != EmptyListObject)
        RaiseExceptionC(Assertion, "list->char-set", "expected a list", List(argv[0]));

    if (sl == 0)
        return(argc == 2 ? argv[1] : EmptyCharSet);

    FCh * s = (FCh *) malloc(sizeof(FCh) * sl);
    if (s == 0)
        RaiseExceptionC(Assertion, "list->char-set", "out of memory", EmptyListObject);

    lst = argv[0];
    for (ulong_t sdx = 0; sdx < sl; sdx++)
    {
        s[sdx] = AsCharacter(First(lst));
        lst = Rest(lst);
    }

    FObject ret = StringToCharSet(sl, s, "list->char-set");
    free(s);

    if (argc == 1)
        return(ret);
    return(CharSetUnion(ret, argv[1], "list->char-set"));
}

Define("string->char-set", StringToCharSetPrimitive)(long_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("string->char-set", argc);
    StringArgCheck("string->char-set", argv[0]);

    if (argc == 2)
        CharSetArgCheck("string->char-set", argv[1]);

    ulong_t sl = StringLength(argv[0]);
    if (sl == 0)
        return(argc == 2 ? argv[1] : EmptyCharSet);

    FCh * s = (FCh *) malloc(sizeof(FCh) * sl);
    if (s == 0)
        RaiseExceptionC(Assertion, "string->char-set", "out of memory", EmptyListObject);

    memcpy(s, AsString(argv[0])->String, sizeof(FCh) * sl);

    FObject ret = StringToCharSet(sl, s, "string->char-set");
    free(s);

    if (argc == 1)
        return(ret);
    return(CharSetUnion(ret, argv[1], "string->char-set"));
}

Define("ucs-range->char-set", UCSRangeToCharSetPrimitive)(long_t argc, FObject argv[])
{
    TwoToFourArgsCheck("ucs-range->char-set", argc);
    FixnumArgCheck("ucs-range->char-set", argv[0]);
    FixnumArgCheck("ucs-range->char-set", argv[1]);

    FCh lower = AsFixnum(argv[0]);
    FCh upper = AsFixnum(argv[1]);
    FObject base = EmptyCharSet;

    if (argc > 2)
        BooleanArgCheck("ucs-range->char-set", argv[2]);

    if (argc > 3)
    {
        CharSetArgCheck("ucs-range->char-set", argv[3]);
        base = argv[3];
    }

    if (upper > 0x10FFFF)
        upper = 0x10FFFF;
    else
        upper -= 1;
    if (lower > upper)
        lower = upper;
    FCharRange r = {lower, upper};
    return(CharSetUnion(MakeCharSet(1, &r, "ucs-range->char-set"), base, "ucs-range->char-set"));
}

Define("char-set-contains?", CharSetContainsPPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("char-set-contains?", argc);
    CharSetArgCheck("char-set-contains?", argv[0]);
    CharacterArgCheck("char-set-contains?", argv[1]);

    return(CharSetMemberP(argv[0], AsCharacter(argv[1])) ? TrueObject : FalseObject);
}

Define("char-set-complement", CharSetComplementPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("char-set-complement", argc);
    CharSetArgCheck("char-set-complement", argv[0]);

    ulong_t nr = NumRanges(argv[0]);

    if (nr == 0)
        return(FullCharSet);

    FCharRange * rg = (FCharRange *) malloc(sizeof(FCharRange) * (nr + 1));
    if (rg == 0)
        RaiseExceptionC(Assertion, "char-set-complement", "out of memory", EmptyListObject);

    FCharRange * r = AsCharSet(argv[0])->Ranges;
    ulong_t ndx = 0;
    ulong_t rl = 0;
    FCh strt = 0;

    if (r[0].Start == 0)
    {
        strt = r[0].End + 1;
        ndx = 1;
    }

    while (ndx < nr)
    {
        rg[rl].Start = strt;
        rg[rl].End = r[ndx].Start - 1;
        rl += 1;

        strt = r[ndx].End + 1;
        ndx += 1;
    }

    if (strt <= 0x10FFFF)
    {
        rg[rl].Start = strt;
        rg[rl].End = 0x10FFFF;
        rl += 1;
    }

    FObject ret = MakeCharSet(rl, rg, "char-set-complement");
    free(rg);

    FAssert(ValidCharSet(ret));

    return(ret);
}

Define("char-set-union", CharSetUnionPrimitive)(long_t argc, FObject argv[])
{
    if (argc == 0)
        return(EmptyCharSet);

    FObject cset = EmptyCharSet;
    for (long_t adx = 0; adx < argc; adx++)
    {
        CharSetArgCheck("char-set-union", argv[adx]);

        cset = CharSetUnion(cset, argv[adx], "char-set-union");
    }

    return(cset);
}

Define("%char-set-intersection", CharSetIntersectionPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%char-set-intersection", argc);
    CharSetArgCheck("%char-set-intersection", argv[0]);
    CharSetArgCheck("%char-set-intersection", argv[1]);

    ulong_t nr1 = NumRanges(argv[0]);
    ulong_t nr2 = NumRanges(argv[1]);

    if (nr1 == 0 || nr2 == 0)
        return(EmptyCharSet);

    FCharRange * r = (FCharRange *) malloc(sizeof(FCharRange) * (nr1 > nr2 ? nr1 : nr2));
    if (r == 0)
        RaiseExceptionC(Assertion, "%char-set-intersection", "out of memory", EmptyListObject);

    FCharRange * r1 = AsCharSet(argv[0])->Ranges;
    FCharRange * r2 = AsCharSet(argv[1])->Ranges;
    ulong_t ndx1 = 0;
    ulong_t ndx2 = 0;
    ulong_t rl = 0;

    while (ndx1 < nr1 && ndx2 < nr2)
    {
        if (r1[ndx1].End < r2[ndx2].Start)
            ndx1 += 1;
        else if (r1[ndx1].Start > r2[ndx2].End)
            ndx2 += 1;
        else if (r1[ndx1].Start < r2[ndx2].Start)
        {
            if (r1[ndx1].End < r2[ndx2].End)
            {
                FAssert(r1[ndx1].End >= r2[ndx2].Start);

                r[rl].Start = r2[ndx2].Start;
                r[rl].End = r1[ndx1].End;
                rl += 1;

                ndx1 += 1;
            }
            else
            {
                r[rl].Start = r2[ndx2].Start;
                r[rl].End = r2[ndx2].End;
                rl += 1;

                ndx2 += 1;
            }
        }
        else
        {
            if (r1[ndx1].End > r2[ndx2].End)
            {
                FAssert(r1[ndx1].Start <= r2[ndx2].End);

                r[rl].Start = r1[ndx1].Start;
                r[rl].End = r2[ndx2].End;
                rl += 1;

                ndx2 += 1;
            }
            else
            {
                r[rl].Start = r1[ndx1].Start;
                r[rl].End = r1[ndx1].End;
                rl += 1;

                ndx1 += 1;
            }
        }
    }

    FObject ret = MakeCharSet(rl, r, "%char-set-intersection");
    free(r);

    FAssert(ValidCharSet(ret));

    return(ret);
}

// ---- Standard CharSets ----

static FCharRange AsciiCharRange[1] =
{
    {0, 127}
};

static FCharRange FullCharRange[2] =
{
    {0, 0xD7FF},
    {0xE000, 0x10FFFF}
};

static FCharRange HexDigitCharRange[3] =
{
    {0x30, 0x39}, // 0 to 9
    {0x41, 0x46}, // A to F
    {0x61, 0x66}  // a to f
};

static FCharRange IsoControlCharRange[2] =
{
    {0, 0x1F},
    {0x7F, 0x9F}
};

// ---- Primitives ----

static FObject Primitives[] =
{
    CharSetPPrimitive,
    CharSetEqualPrimitive,
    CharSetSubsetPrimitive,
    CharSetHashPrimitive,
    CharSetCursorPrimitive,
    CharSetCursorNextPrimitive,
    ListToCharSetPrimitive,
    StringToCharSetPrimitive,
    UCSRangeToCharSetPrimitive,
    CharSetContainsPPrimitive,
    CharSetComplementPrimitive,
    CharSetUnionPrimitive,
    CharSetIntersectionPrimitive
};

static void StandardCharSet(const char * sym, FObject cset)
{
    FAssert(CharSetP(cset));
    FAssert(ValidCharSet(cset));

    LibraryExport(BedrockLibrary, EnvironmentSetC(Bedrock, sym, cset));
}

void SetupCharSets()
{
    RegisterRoot(&EmptyCharSet, "char-set:empty");
    EmptyCharSet = MakeCharSet(0, 0, "char-set:empty");
    StandardCharSet("char-set:empty", EmptyCharSet);

    RegisterRoot(&FullCharSet, "char-set:full");
    FullCharSet = MakeCharSet(sizeof(FullCharRange) / sizeof(FCharRange), FullCharRange,
            "char-set:full");
    StandardCharSet("char-set:full", FullCharSet);

    StandardCharSet("char-set:ascii",
            MakeCharSet(sizeof(AsciiCharRange) / sizeof(FCharRange), AsciiCharRange,
                    "char-set:ascii"));

    StandardCharSet("char-set:lower-case",
            MakeCharSet(sizeof(LowerCaseCharRange) / sizeof(FCharRange), LowerCaseCharRange,
                    "char-set:lower-case"));

    StandardCharSet("char-set:upper-case",
            MakeCharSet(sizeof(UpperCaseCharRange) / sizeof(FCharRange), UpperCaseCharRange,
                    "char-set:upper-case"));

    StandardCharSet("char-set:title-case",
            MakeCharSet(sizeof(TitleCaseCharRange) / sizeof(FCharRange), TitleCaseCharRange,
                    "char-set:title-case"));

    StandardCharSet("char-set:letter",
            MakeCharSet(sizeof(LetterCharRange) / sizeof(FCharRange), LetterCharRange,
                    "char-set:letter"));

    StandardCharSet("char-set:digit",
            MakeCharSet(sizeof(DigitCharRange) / sizeof(FCharRange), DigitCharRange,
                    "char-set:digit"));

    StandardCharSet("char-set:hex-digit",
            MakeCharSet(sizeof(HexDigitCharRange) / sizeof(FCharRange), HexDigitCharRange,
                    "char-set:hex-digit"));

    StandardCharSet("char-set:whitespace",
            MakeCharSet(sizeof(WhitespaceCharRange) / sizeof(FCharRange), WhitespaceCharRange,
                    "char-set:whitespace"));

    StandardCharSet("char-set:iso-control",
            MakeCharSet(sizeof(IsoControlCharRange) / sizeof(FCharRange), IsoControlCharRange,
                    "char-set:iso-control"));

    StandardCharSet("char-set:punctuation",
            MakeCharSet(sizeof(PunctuationCharRange) / sizeof(FCharRange), PunctuationCharRange,
                    "char-set:punctuation"));

    StandardCharSet("char-set:symbol",
            MakeCharSet(sizeof(SymbolCharRange) / sizeof(FCharRange), SymbolCharRange,
                    "char-set:symbol"));

    StandardCharSet("char-set:blank",
            MakeCharSet(sizeof(BlankCharRange) / sizeof(FCharRange), BlankCharRange,
                    "char-set:blank"));

    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
