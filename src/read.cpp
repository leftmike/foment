/*

Foment

*/

#ifdef FOMENT_WINDOWS
#include <windows.h>
#endif // FOMENT_WINDOWS
#ifdef FOMENT_UNIX
#include <pthread.h>
#endif // FOMENT_UNIX
#include <string.h>
#ifdef FOMENT_BSD
#include <stdlib.h>
#else // FOMENT_BSD
#include <malloc.h>
#endif // FOMENT_BSD
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "unicode.hpp"

#define MAXIMUM_IDENTIFIER 256
#define MAXIMUM_NUMBER 256
#define MAXIMUM_NAME 32

// ---- Datum Reference ----

typedef struct
{
    FRecord Record;
    FObject Label;
} FDatumReference;

#define AsDatumReference(obj) ((FDatumReference *) (obj))
#define DatumReferenceP(obj) RecordP(obj, R.DatumReferenceRecordType)

static const char * DatumReferenceFieldsC[] = {"label"};

static FObject MakeDatumReference(FObject lbl)
{
    FAssert(sizeof(FDatumReference) == sizeof(DatumReferenceFieldsC) + sizeof(FRecord));
    FAssert(FixnumP(lbl));

    FDatumReference * dref = (FDatumReference *) MakeRecord(R.DatumReferenceRecordType);
    dref->Label = lbl;

    return(dref);
}

// ----------------

static int_t NumericP(FCh ch)
{
    int_t dv = DigitValue(ch);
    if (dv < 0 || dv > 9)
        return(0);
    return(1);
}

static int_t IdentifierInitialP(FCh ch)
{
    return(AlphabeticP(ch) || ch == '!' || ch == '$' || ch == '%' || ch == '&'
             || ch =='*' || ch == '/' || ch == ':' || ch == '<' || ch == '=' || ch == '>'
             || ch == '?'|| ch == '^' || ch == '_' || ch == '~' || ch == '@');
}

int_t IdentifierSubsequentP(FCh ch)
{
    return(IdentifierInitialP(ch) || NumericP(ch) || ch == '+' || ch == '-' || ch == '.');
}

static int_t DelimiterP(FCh ch)
{
    return(WhitespaceP(ch) || ch == '|' || ch == '(' || ch == ')' || ch == '"' || ch == ';');
}

static int_t SignSubsequentP(FCh ch)
{
    return(IdentifierInitialP(ch) || ch == '-' || ch == '+');
}

static int_t DotSubsequentP(FCh ch)
{
    return(SignSubsequentP(ch) || ch == '.');
}

#define DotObject ((FObject) -1)
#define EolObject ((FObject *) -2)

static FCh ReadStringHexChar(FObject port)
{
    FAlive ap(&port);
    FCh s[16];
    int_t sl = 2;
    FCh ch;

    for (;;)
    {
        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading string",
                    List(port));

        if (ch == ';')
            break;

        s[sl] = ch;
        sl += 1;
        if (sl == sizeof(s) / sizeof(FCh))
            RaiseExceptionC(R.Lexical, "read",
                    "missing ; to terminate \\x<hex-value> in string", List(port));
    }

    FObject n = StringToNumber(s + 2, sl - 2, 16);

    if (FixnumP(n) == 0)
    {
        s[0] = '\\';
        s[1] = 'x';

        RaiseExceptionC(R.Lexical, "read", "expected a valid hexidecimal value for a character",
                List(port, MakeString(s, sl)));
    }

    return((FCh) AsFixnum(n));
}

static FObject ReadStringLiteral(FObject port, FCh tch)
{
    FAlive ap(&port);
    FCh s[512];
    int_t sl = 0;
    FCh ch;

    FAssert(tch == '"' || tch == '|');

    for (;;)
    {
        if (ReadCh(port, &ch) == 0)
            goto UnexpectedEof;

Again:

        if (ch == tch)
            break;

        if (ch == '\\')
        {
            if (ReadCh(port, &ch) == 0)
                goto UnexpectedEof;

            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
            {
                while (ch == ' ' || ch == '\t')
                {
                    if (ReadCh(port, &ch) == 0)
                        goto UnexpectedEof;
                }

                while (ch == '\r' || ch == '\n')
                {
                    if (ReadCh(port, &ch) == 0)
                        goto UnexpectedEof;
                }

                while (ch == ' ' || ch == '\t')
                {
                    if (ReadCh(port, &ch) == 0)
                        goto UnexpectedEof;
                }

                goto Again;
            }

            switch (ch)
            {
            case 'a': ch = 0x0007; break;
            case 'b': ch = 0x0008; break;
            case 't': ch = 0x0009; break;
            case 'n': ch = 0x000A; break;
            case 'r': ch = 0x000D; break;
            case '"': ch = 0x0022; break;
            case '\\': ch = 0x005C; break;
            case '|': ch = 0x007C; break;
            case 'x':
                ch = ReadStringHexChar(port);
                break;
            default:
                RaiseExceptionC(R.Lexical, "read", "unexpected character following \\",
                        List(port, MakeCharacter(ch)));
            }
        }

        s[sl] = ch;
        sl += 1;
        if (sl == sizeof(s) / sizeof(FCh))
            RaiseExceptionC(R.Restriction, "read", "string too long", List(port));
    }

    return(MakeString(s, sl));

UnexpectedEof:
    RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading string", List(port));
    return(NoValueObject);
}

static FObject ReadNumber(FObject port, FCh * s, int_t sdx, FFixnum rdx, int_t df)
{
    FAlive ap(&port);
    FCh ch;

    for (;;)
    {
        if (PeekCh(port, &ch) == 0)
            break;

        if (df)
        {
            if (NumericP(ch) == 0)
                break;
        }
        else if (IdentifierSubsequentP(ch) == 0)
            break;

        s[sdx] = ch;
        sdx += 1;
        if (sdx == MAXIMUM_NUMBER)
            RaiseExceptionC(R.Restriction, "read", "number too long", List(port));

        ReadCh(port, &ch);
    }

    FObject n = StringToNumber(s, sdx, rdx);
    if (n == FalseObject)
        RaiseExceptionC(R.Lexical, "read", "expected a valid number",
                List(port, MakeString(s, sdx)));

    return(n);
}

static int_t ReadName(FObject port, FCh ch, FCh * s)
{
    FAlive ap(&port);
    int_t sl;

    sl = 0;
    for (;;)
    {
        s[sl] = ch;
        sl += 1;
        if (sl == MAXIMUM_NAME)
            RaiseExceptionC(R.Restriction, "read", "name too long", List(port));

        if (PeekCh(port, &ch) == 0)
            break;

        if (IdentifierSubsequentP(ch) == 0)
            break;

        ReadCh(port, &ch);
    }

    return(sl);
}

static FObject ReadIdentifier(FObject port, FCh * s, int_t sdx, int_t mbnf)
{
    FAlive ap(&port);
    int_t ln;
    FCh ch;

    if (WantIdentifiersPortP(port))
        ln = GetLineColumn(port, 0);

    for (;;)
    {
        if (PeekCh(port, &ch) == 0)
            break;

        if (IdentifierSubsequentP(ch) == 0)
            break;

        s[sdx] = ch;
        sdx += 1;
        if (sdx == MAXIMUM_IDENTIFIER)
            RaiseExceptionC(R.Restriction, "read", "symbol too long", List(port));

        ReadCh(port, &ch);
    }

    if (mbnf)
    {
        FObject n = StringToNumber(s, sdx, 10);
        if (n != FalseObject)
            return(n);
    }

    FObject sym = FoldcasePortP(port) ? StringToSymbol(FoldcaseString(MakeString(s, sdx)))
            : StringLengthToSymbol(s, sdx);
    if (WantIdentifiersPortP(port))
        return(MakeIdentifier(sym, ln));
    return(sym);
}

static FObject ReadList(FObject port, FObject * pdlht);
static FObject Read(FObject port, int_t eaf, int_t rlf, FObject * pdlht);
static FObject ReadSharp(FObject port, int_t eaf, int_t rlf, FObject * pdlht)
{
    FAlive ap(&port);
    FCh ch;

    if (ReadCh(port, &ch) == 0)
        RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading #", List(port));

    if (ch == 't' || ch == 'f')
    {
        FCh s[MAXIMUM_NAME];

        int_t sl = ReadName(port, ch, s);

        if (StringCEqualP("t", s, sl) || StringCEqualP("true", s, sl))
            return(TrueObject);

        if (StringCEqualP("f", s, sl) || StringCEqualP("false", s, sl))
            return(FalseObject);

        RaiseExceptionC(R.Lexical, "read", "unexpected character(s) following #",
                List(port, MakeString(s, sl)));
    }
    else if (ch == '\\')
    {
        FCh s[MAXIMUM_NAME];

        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading #\\", List(port));

        if (IdentifierInitialP(ch) == 0)
            return(MakeCharacter(ch));

        int_t sl = ReadName(port, ch, s);

        if (sl == 1)
            return(MakeCharacter(ch));

        if (s[0] == 'x')
        {
            FAssert(sl > 1);

            FObject n = StringToNumber(s + 1, sl - 1, 16);
            if (FixnumP(n) == 0 || AsFixnum(n) < 0)
                RaiseExceptionC(R.Lexical, "read", "expected #\\x<hex value>",
                        List(port, MakeString(s, sl)));

            return(MakeCharacter(AsFixnum(n)));
        }

        if (StringCEqualP("alarm", s, sl))
            return(MakeCharacter(0x0007));

        if (StringCEqualP("backspace", s, sl))
            return(MakeCharacter(0x0008));

        if (StringCEqualP("delete", s, sl))
            return(MakeCharacter(0x007F));

        if (StringCEqualP("escape", s, sl))
            return(MakeCharacter(0x001B));

        if (StringCEqualP("newline", s, sl))
            return(MakeCharacter(0x000A));

        if (StringCEqualP("null", s, sl))
            return(MakeCharacter(0x0000));

        if (StringCEqualP("return", s, sl))
            return(MakeCharacter(0x000D));

        if (StringCEqualP("space", s, sl))
            return(MakeCharacter(' '));

        if (StringCEqualP("tab", s, sl))
            return(MakeCharacter(0x0009));

        RaiseExceptionC(R.Lexical, "read", "unexpected character name",
                List(port, MakeString(s, sl)));
    }
    else if (ch == 'b' || ch == 'B' || ch == 'o' || ch == 'O' || ch == 'd' || ch == 'D'
            || ch == 'x' || ch == 'X' || ch =='i' || ch == 'I' || ch == 'e' || ch == 'E')
    {
        FCh s[MAXIMUM_NUMBER];

        s[0] = '#';
        s[1] = ch;
        int_t sdx = 2;

        if (PeekCh(port, &ch) && ch == '#')
        {
            ReadCh(port, &ch);

            if (PeekCh(port, &ch) == 0)
                RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading number",
                        List(port));

            if (ch == 'b' || ch == 'B' || ch == 'o' || ch == 'O' || ch == 'd' || ch == 'D'
                    || ch == 'x' || ch == 'X' || ch =='i' || ch == 'I' || ch == 'e' || ch == 'E')
            {
                ReadCh(port, &ch);

                FAssert(sdx + 2 < MAXIMUM_NUMBER);

                s[sdx] = '#';
                sdx += 1;
                s[sdx] = ch;
                sdx += 1;
            }
            else
                RaiseExceptionC(R.Lexical, "read", "unexpected character following #",
                        List(port, MakeCharacter(ch)));
        }

        return(ReadNumber(port, s, sdx, 10, 0));
    }
    else if (ch ==  '(')
        return(ListToVector(ReadList(port, pdlht)));
    else if (ch == 'u')
    {
        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading bytevector",
                    List(port));
        if (ch != '8')
            RaiseExceptionC(R.Lexical, "read", "expected #\\u8(", List(port));

        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading bytevector",
                    List(port));
        if (ch != '(')
            RaiseExceptionC(R.Lexical, "read", "expected #\\u8(", List(port));
        return(U8ListToBytevector(ReadList(port, pdlht)));
    }
    else if (ch == ';')
    {
        Read(port, 0, 0, pdlht);

        return(Read(port, eaf, rlf, pdlht));
    }
    else if (ch == '|')
    {
        int_t lvl = 1;

        FCh pch = 0;
        while (lvl > 0)
        {
            if (ReadCh(port, &ch) == 0)
                RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file in block comment",
                        List(port));

            if (pch == '#' && ch == '|')
                lvl += 1;
            else if (pch == '|' && ch == '#')
                lvl -= 1;

            pch = ch;
        }

        return(Read(port, eaf, rlf, pdlht));
    }
    else if (ch == '!')
    {
        FCh s[MAXIMUM_NAME];

        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading #!", List(port));

        if (IdentifierInitialP(ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected character following #!",
                    List(port, MakeCharacter(ch)));

        int_t sl = ReadName(port, ch, s);

        if (StringCEqualP("fold-case", s, sl))
            FoldcasePort(port, 1);
        else if (StringCEqualP("no-fold-case", s, sl))
            FoldcasePort(port, 0);
        else
            RaiseExceptionC(R.Lexical, "read", "unknown directive #!<name>",
                    List(port, MakeString(s, sl)));

        return(Read(port, eaf, rlf, pdlht));
    }
    else if (NumericP(ch))
    {
        FCh s[MAXIMUM_NUMBER];
        s[0] = ch;
        FObject n = ReadNumber(port, s, 1, 10, 1);

        if (FixnumP(n) == 0)
            RaiseExceptionC(R.Lexical, "read", "expected an integer for <n>: #<n>= and #<n>#",
                    List(port));

        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading #<n>= or #<n>#",
                    List(port));

        if (ch == '=')
        {
            FObject obj = Read(port, 0, 0, pdlht);

            if (HashtableP(*pdlht) == 0)
                *pdlht = MakeHashtable(31);

            if (HashtableContainsP(*pdlht, n, EqP, EqHash))
                RaiseExceptionC(R.Lexical, "read", "duplicate datum label", List(port, n));

            HashtableSet(*pdlht, n, obj, EqP, EqHash);
            return(obj);
        }
        else if (ch == '#')
            return(MakeDatumReference(n));

        RaiseExceptionC(R.Lexical, "read", "expected #<n>= or #<n>#", List(port,
                MakeCharacter(ch)));
    }

    RaiseExceptionC(R.Lexical, "read", "unexpected character following #",
            List(port, MakeCharacter(ch)));

    return(NoValueObject);
}

static FObject Read(FObject port, int_t eaf, int_t rlf, FObject * pdlht)
{
    FAlive ap(&port);
    FCh ch;

    for (;;)
    {
        if (ReadCh(port, &ch) == 0)
            break;

        if (ch == ';')
        {
            do
            {
                if (ReadCh(port, &ch) == 0)
                    goto Eof;
            }
            while (ch != '\n' && ch != '\r');
        }
        else
        {
            switch (ch)
            {
            case '#':
                return(ReadSharp(port, eaf, rlf, pdlht));

            case '"':
                return(ReadStringLiteral(port, '"'));

            case '|':
            {
                int_t ln;

                if (WantIdentifiersPortP(port))
                    ln = GetLineColumn(port, 0);

                FObject sym = FoldcasePortP(port)
                        ? StringToSymbol(FoldcaseString(ReadStringLiteral(port, '|')))
                        : StringToSymbol(ReadStringLiteral(port, '|'));
                return(WantIdentifiersPortP(port) ? MakeIdentifier(sym, ln) : sym);
            }

            case '(':
                return(ReadList(port, pdlht));

            case ')':
                if (rlf)
                    return(EolObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected )", List(port));
                break;

            case '.':
                if (PeekCh(port, &ch) == 0)
                    RaiseExceptionC(R.Lexical, "read",
                            "unexpected end-of-file reading dot", List(port));

                if (DotSubsequentP(ch))
                {
                    FCh s[MAXIMUM_IDENTIFIER];
                    s[0] = '.';
                    return(ReadIdentifier(port, s, 1, 0));
                }
                else if (DelimiterP(ch) == 0)
                {
                    FCh s[MAXIMUM_NUMBER];
                    s[0] = '.';
                    return(ReadNumber(port, s, 1, 10, 0));
                }

                if (rlf)
                    return(DotObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected dotted pair", List(port));
                break;

            case '\'':
            {
                FObject obj = Read(port, 0, 0, pdlht);
                return(MakePair(WantIdentifiersPortP(port) ? MakeIdentifier(R.QuoteSymbol,
                        GetLineColumn(port, 0)) : R.QuoteSymbol, MakePair(obj, EmptyListObject)));
            }

            case '`':
            {
                FObject obj = Read(port, 0, 0, pdlht);
                return(MakePair(WantIdentifiersPortP(port)
                        ? MakeIdentifier(R.QuasiquoteSymbol, GetLineColumn(port, 0))
                        : R.QuasiquoteSymbol, MakePair(obj, EmptyListObject)));
            }

            case ',':
            {
                if (PeekCh(port, &ch) == 0)
                    RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading unquote",
                            List(port));

                FObject sym = R.UnquoteSymbol;
                FAlive as(&sym);
                if (ch == '@')
                {
                    ReadCh(port, &ch);
                    sym = R.UnquoteSplicingSymbol;
                }

                FObject obj = Read(port, 0, 0, pdlht);
                return(MakePair(WantIdentifiersPortP(port)
                        ? MakeIdentifier(sym, GetLineColumn(port, 0)) : sym,
                        MakePair(obj, EmptyListObject)));
            }

            case '-':
            case '+':
            {
                FCh pch;

                if (PeekCh(port, &pch) == 0 || SignSubsequentP(pch) || DelimiterP(pch))
                {
                    FCh s[MAXIMUM_IDENTIFIER];
                    s[0] = ch;
                    return(ReadIdentifier(port, s, 1, 1));
                }

                if (pch == '.')
                {
                    FCh ch2;
                    ReadCh(port, &ch2);

                    if (PeekCh(port, &pch) == 0)
                        RaiseExceptionC(R.Lexical, "read",
                                "unexpected end-of-file reading identifier or number",
                                List(port));

                    if (DotSubsequentP(pch))
                    {
                        FCh s[MAXIMUM_IDENTIFIER];
                        s[0] = ch;
                        s[1] = ch2;
                        return(ReadIdentifier(port, s, 2, 0));
                    }
                    else
                    {
                        FCh s[MAXIMUM_NUMBER];
                        s[0] = ch;
                        s[1] = ch2;
                        return(ReadNumber(port, s, 2, 10, 0));
                    }
                }
                else
                {
                    FCh s[MAXIMUM_NUMBER];
                    s[0] = ch;
                    return(ReadNumber(port, s, 1, 10, 0));
                }
            }

            default:
                if (IdentifierInitialP(ch))
                {
                    FCh s[MAXIMUM_IDENTIFIER];
                    s[0] = ch;
                    return(ReadIdentifier(port, s, 1, 0));
                }
                else if (NumericP(ch))
                {
                    FCh s[MAXIMUM_NUMBER];
                    s[0] = ch;
                    return(ReadNumber(port, s, 1, 10, 0));
                }

                if (WhitespaceP(ch) == 0)
                    RaiseExceptionC(R.Lexical, "read", "unexpected character",
                            List(port, MakeCharacter(ch)));
                break;
            }
        }
    }

Eof:

    if (eaf == 0)
        RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading list or vector",
                List(port));
    return(EndOfFileObject);
}

static FObject ReadList(FObject port, FObject * pdlht)
{
    FAlive ap(&port);
    FObject obj = Read(port, 0, 1, pdlht);
    FAlive ao(&obj);

    if (obj == EolObject)
        return(EmptyListObject);

    if (obj == DotObject)
    {
        obj = Read(port, 0, 0, pdlht);

        if (Read(port, 0, 1, pdlht) != EolObject)
            RaiseExceptionC(R.Lexical, "read", "bad dotted pair", List(port));
        return(obj);
    }

    return(MakePair(obj, ReadList(port, pdlht)));
}

static FObject ResolveReference(FObject port, FObject ref, FObject dlht)
{
    FAssert(DatumReferenceP(ref));
    FAssert(FixnumP(AsDatumReference(ref)->Label));

    FObject obj = HashtableRef(dlht, AsDatumReference(ref)->Label, NotFoundObject, EqP, EqHash);
    if (obj == NotFoundObject)
        RaiseExceptionC(R.Lexical, "read", "datum reference to unknown label",
                List(port, AsDatumReference(ref)->Label));

    return(obj);
}

static void ResolveDatumReferences(FObject port, FObject obj, FObject dlht)
{
    while (PairP(obj))
    {
        if (DatumReferenceP(First(obj)))
            SetFirst(obj, ResolveReference(port, First(obj), dlht));
        else if (PairP(First(obj)) || VectorP(First(obj)))
            ResolveDatumReferences(port, First(obj), dlht);

        if (DatumReferenceP(Rest(obj)))
        {
            SetRest(obj, ResolveReference(port, Rest(obj), dlht));
            break;
        }

        obj = Rest(obj);
    }

    if (VectorP(obj))
    {
        for (uint_t idx = 0; idx < VectorLength(obj); idx++)
        {
            FObject val = AsVector(obj)->Vector[idx];
            if (DatumReferenceP(val))
                ModifyVector(obj, idx, ResolveReference(port, val, dlht));
            else if (PairP(val) || VectorP(val))
                ResolveDatumReferences(port, val, dlht);
        }
    }
}

FObject Read(FObject port)
{
    FAssert(InputPortP(port) && InputPortOpenP(port));

    FObject dlht = NoValueObject;
    FAlive ap(&port);
    FAlive adlht(&dlht);

    FObject obj = Read(port, 1, 0, &dlht);
    if (HashtableP(dlht))
        ResolveDatumReferences(port, obj, dlht);

    return(obj);
}

// ---- Input ----

Define("read", ReadPrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("read", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("read", port);

    return(Read(port));
}

Define("read-char", ReadCharPrimitive)(int_t argc, FObject argv[])
{
    FCh ch;

    ZeroOrOneArgsCheck("read-char", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("read-char", port);

    return(ReadCh(port, &ch) == 0 ? EndOfFileObject : MakeCharacter(ch));
}

Define("peek-char", PeekCharPrimitive)(int_t argc, FObject argv[])
{
    FCh ch;

    ZeroOrOneArgsCheck("peek-char", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("peek-char", port);

    return(PeekCh(port, &ch) == 0 ? EndOfFileObject : MakeCharacter(ch));
}

Define("read-line", ReadLinePrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("read-line", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("read-line", port);

    return(ReadLine(port));
}

Define("eof-object?", EofObjectPPrimitive)(int_t argc, FObject argv[])
{
    OneArgCheck("eof-object?", argc);

    return(argv[0] == EndOfFileObject ? TrueObject : FalseObject);
}

Define("eof-object", EofObjectPrimitive)(int_t argc, FObject argv[])
{
    ZeroArgsCheck("eof-object", argc);

    return(EndOfFileObject);
}

Define("char-ready?", CharReadyPPrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("char-ready?", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("char-ready?", port);

    return(CharReadyP(port) ? TrueObject : FalseObject);
}

Define("read-string", ReadStringPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("read-string", argc);
    NonNegativeArgCheck("read-string", argv[0], 0);
    FObject port = (argc == 2 ? argv[1] : CurrentInputPort());
    TextualInputPortArgCheck("read-string", port);

    return(ReadString(port, AsFixnum(argv[0])));
}

Define("read-u8", ReadU8Primitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("read-u8", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    BinaryInputPortArgCheck("read-u8", port);

    FByte b;
    return(ReadBytes(port, &b, 1) == 0 ? EndOfFileObject : MakeFixnum(b));
}

Define("peek-u8", PeekU8Primitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("peek-u8", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    BinaryInputPortArgCheck("peek-u8", port);

    FByte b;
    return(PeekByte(port, &b) == 0 ? EndOfFileObject : MakeFixnum(b));
}

Define("u8-ready?", U8ReadyPPrimitive)(int_t argc, FObject argv[])
{
    ZeroOrOneArgsCheck("u8-ready?", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    BinaryInputPortArgCheck("u8-ready?", port);

    return(ByteReadyP(port) ? TrueObject : FalseObject);
}

Define("read-bytevector", ReadBytevectorPrimitive)(int_t argc, FObject argv[])
{
    OneOrTwoArgsCheck("read-bytevector", argc);
    NonNegativeArgCheck("read-bytevector", argv[0], 0);
    FObject port = (argc == 2 ? argv[1] : CurrentInputPort());
    BinaryInputPortArgCheck("read-bytevector", port);

    int_t bvl = AsFixnum(argv[0]);
    FByte b[128];
    FByte * ptr;
    if (bvl <= (int) sizeof(b))
        ptr = b;
    else
    {
        ptr = (FByte *) malloc(bvl);
        if (ptr == 0)
            RaiseExceptionC(R.Restriction, "read-bytevector!", "insufficient memory",
                    List(argv[0]));
    }

    int_t rl = ReadBytes(port, ptr, bvl);
    if (rl == 0)
        return(EndOfFileObject);

    FObject bv = MakeBytevector(rl);
    memcpy(AsBytevector(bv)->Vector, ptr, rl);

    if (ptr != b)
        free(ptr);

    return(bv);
}

Define("read-bytevector!", ReadBytevectorModifyPrimitive)(int_t argc, FObject argv[])
{
    OneToFourArgsCheck("read-bytevector!", argc);
    BytevectorArgCheck("read-bytevector!", argv[0]);
    FObject port = argc > 1 ? argv[1] : CurrentInputPort();
    BinaryInputPortArgCheck("read-bytevector!", port);

    int_t strt;
    int_t end;
    if (argc > 2)
    {
        IndexArgCheck("read-bytevector!", argv[2], BytevectorLength(argv[0]));

        strt = AsFixnum(argv[2]);

        if (argc > 3)
        {
            EndIndexArgCheck("read-bytevector!", argv[3], strt, BytevectorLength(argv[0]));

            end = AsFixnum(argv[3]);
        }
        else
            end = (int_t) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int_t) BytevectorLength(argv[0]);
    }

    FByte b[128];
    FByte * ptr;
    if (end - strt <= (int) sizeof(b))
        ptr = b;
    else
    {
        ptr = (FByte *) malloc(end - strt);
        if (ptr == 0)
            RaiseExceptionC(R.Restriction, "read-bytevector", "insufficient memory",
                    List(MakeFixnum(end - strt)));
    }

    int_t rl = ReadBytes(port, ptr, end - strt);
    if (rl == 0)
        return(EndOfFileObject);

    memcpy(AsBytevector(argv[0])->Vector + strt, ptr, end - strt);
    return(MakeFixnum(rl));
}

static FPrimitive * Primitives[] =
{
    &ReadPrimitive,
    &ReadCharPrimitive,
    &PeekCharPrimitive,
    &ReadLinePrimitive,
    &EofObjectPPrimitive,
    &EofObjectPrimitive,
    &CharReadyPPrimitive,
    &ReadStringPrimitive,
    &ReadU8Primitive,
    &PeekU8Primitive,
    &U8ReadyPPrimitive,
    &ReadBytevectorPrimitive,
    &ReadBytevectorModifyPrimitive
};

void SetupRead()
{
    FAssert(MAXIMUM_NUMBER == MAXIMUM_IDENTIFIER);

    R.DatumReferenceRecordType = MakeRecordTypeC("datum-reference",
            sizeof(DatumReferenceFieldsC) / sizeof(char *), DatumReferenceFieldsC);

    for (uint_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
