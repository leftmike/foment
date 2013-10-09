/*

Foment

*/

#include <windows.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "unicode.hpp"

static int NumericP(FCh ch)
{
    int dv = DigitValue(ch);
    if (dv < 0 || dv > 9)
        return(0);
    return(1);
}

static int SymbolCharP(FCh ch)
{
    return(AlphabeticP(ch) || NumericP(ch) || ch == '!' || ch == '$' || ch == '%' || ch == '&'
             || ch =='*' || ch == '+' || ch == '-' || ch == '.' || ch == '/' || ch == ':'
             || ch == '<' || ch == '=' || ch == '>' || ch == '?' || ch == '@' || ch == '^'
             || ch == '_' || ch == '~');
}

#define DotObject ((FObject) -1)
#define EolObject ((FObject *) -2)

static FCh DoReadStringHexChar(FObject port)
{
    FCh s[16];
    int sl = 2;
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

    FFixnum n;

    if (StringToNumber(s + 2, sl - 2, &n, 16) == 0)
    {
        s[0] = '\\';
        s[1] = 'x';

        RaiseExceptionC(R.Lexical, "read", "expected a valid hexidecimal value for a character",
                List(port, MakeString(s, sl)));
    }

    return((FCh) n);
}

static FObject DoReadString(FObject port, FCh tch)
{
    FCh s[512];
    int sl = 0;
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
                ch = DoReadStringHexChar(port);
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

static int DoReadToken(FObject port, FCh ch, FCh * s, int msl)
{
    int sl;

    sl = 0;
    for (;;)
    {
        s[sl] = ch;
        sl += 1;
        if (sl == msl)
            RaiseExceptionC(R.Restriction, "read", "symbol or number too long", List(port));

        if (PeekCh(port, &ch) == 0)
            break;

        if (SymbolCharP(ch) == 0)
            break;

        ReadCh(port, &ch);
    }

    return(sl);
}

static FObject DoReadSymbolOrNumber(FObject port, int ch, int rif, int fcf)
{
    FCh s[256];
    FFixnum n;
    int ln;

    if (rif)
        ln = GetLocation(port);

    int sl = DoReadToken(port, ch, s, sizeof(s) / sizeof(FCh));

    if (StringToNumber(s, sl, &n, 10))
        return(MakeFixnum(n));

    FObject sym = fcf ? StringToSymbol(FoldcaseString(MakeString(s, sl)))
            : StringLengthToSymbol(s, sl);
    if (rif)
        return(MakeIdentifier(sym, ln));
    return(sym);
}

static FObject DoReadList(FObject port, int rif, int fcf);
static FObject DoRead(FObject port, int eaf, int rlf, int rif, int fcf);
static FObject DoReadSharp(FObject port, int eaf, int rlf, int rif, int fcf)
{
    FCh ch;

    if (ReadCh(port, &ch) == 0)
        RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading #", List(port));

    if (ch == 't' || ch == 'f')
    {
        FCh s[16];

        int sl = DoReadToken(port, ch, s, sizeof(s) / sizeof(FCh));

        if (StringCEqualP("t", s, sl) || StringCEqualP("true", s, sl))
            return(TrueObject);

        if (StringCEqualP("f", s, sl) || StringCEqualP("false", s, sl))
            return(FalseObject);

        RaiseExceptionC(R.Lexical, "read", "unexpected character(s) following #",
                List(port, MakeString(s, sl)));
    }
    else if (ch == '\\')
    {
        FCh s[32];

        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading #\\", List(port));

        if (SymbolCharP(ch) == 0)
            return(MakeCharacter(ch));

        int sl = DoReadToken(port, ch, s, sizeof(s) / sizeof(FCh));

        if (sl == 1)
            return(MakeCharacter(ch));

        if (s[0] == 'x')
        {
            FAssert(sl > 1);

            FFixnum n;

            if (StringToNumber(s + 1, sl - 1, &n, 16) == 0 || n < 0)
                RaiseExceptionC(R.Lexical, "read", "expected #\\x<hex value>",
                        List(port, MakeString(s, sl)));

            return(MakeCharacter(n));
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
    else if (ch == 'b' || ch == 'o' || ch == 'd' || ch == 'x')
    {
        FCh s[32];
        FFixnum n;
        int b = (ch == 'b' ? 2 : (ch == 'o' ? 8 : (ch == 'd' ? 10 : 16)));

        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading constant",
                    List(port));

        int sl = DoReadToken(port, ch, s, sizeof(s) / sizeof(FCh));

        if (StringToNumber(s, sl, &n, b) == 0)
            RaiseExceptionC(R.Lexical, "read", "expected a numerical constant",
                    List(port, MakeString(s, sl)));

        return(MakeFixnum(n));
    }
    else if (ch ==  '(')
        return(ListToVector(DoReadList(port, rif, fcf)));
    else if (ch == 'u')
    {
        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading bytevector",
                    List(port));
        if (ch != '8')
            RaiseExceptionC(R.Lexical, "read", "expected #\u8(", List(port));

        if (ReadCh(port, &ch) == 0)
            RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading bytevector",
                    List(port));
        if (ch != '(')
            RaiseExceptionC(R.Lexical, "read", "expected #\u8(", List(port));
        return(U8ListToBytevector(DoReadList(port, rif, fcf)));
    }
    else if (ch == ';')
    {
        DoRead(port, 0, 0, 0, 0);

        return(DoRead(port, eaf, rlf, rif, fcf));
    }
    else if (ch == '|')
    {
        int lvl = 1;

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

        return(DoRead(port, eaf, rlf, rif, fcf));
    }

    RaiseExceptionC(R.Lexical, "read", "unexpected character following #",
            List(port, MakeCharacter(ch)));

    return(NoValueObject);
}

static FObject DoRead(FObject port, int eaf, int rlf, int rif, int fcf)
{
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
                return(DoReadSharp(port, eaf, rlf, rif, fcf));

            case '"':
                return(DoReadString(port, '"'));

            case '|':
            {
                int ln;

                if (rif)
                    ln = GetLocation(port);

                FObject sym = fcf ? StringToSymbol(FoldcaseString(DoReadString(port, '|')))
                        : StringToSymbol(DoReadString(port, '|'));
                return(rif ? MakeIdentifier(sym, ln) : sym);
            }

            case '(':
                return(DoReadList(port, rif, fcf));

            case ')':
                if (rlf)
                    return(EolObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected )", List(port));
                break;

            case '.':
                if (PeekCh(port, &ch) == 0)
                    RaiseExceptionC(R.Lexical, "read",
                            "unexpected end-of-file reading dotted pair", List(port));
                if (ch == '.')
                {
                    ReadCh(port, &ch);

                    if (ReadCh(port, &ch) == 0)
                        RaiseExceptionC(R.Lexical, "read",
                                "unexpected end-of-file reading ...", List(port));
                        if (ch != '.')
                            RaiseExceptionC(R.Lexical, "read", "expected ...", List(port));
                    return(rif ? MakeIdentifier(R.EllipsisSymbol, GetLocation(port))
                            : R.EllipsisSymbol);
                }

                if (rlf)
                    return(DotObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected dotted pair", List(port));
                break;

            case '\'':
            {
                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair( rif ? MakeIdentifier(R.QuoteSymbol,
                        GetLocation(port)) : R.QuoteSymbol, MakePair(obj, EmptyListObject)));
            }

            case '`':
            {
                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair(
                        rif ? MakeIdentifier(R.QuasiquoteSymbol, GetLocation(port))
                        : R.QuasiquoteSymbol, MakePair(obj, EmptyListObject)));
            }

            case ',':
            {
                if (PeekCh(port, &ch) == 0)
                    RaiseExceptionC(R.Lexical, "read", "unexpected end-of-file reading unquote",
                            List(port));

                FObject sym;
                if (ch == '@')
                {
                    ReadCh(port, &ch);
                    sym = R.UnquoteSplicingSymbol;
                }
                else
                    sym = R.UnquoteSymbol;

                FObject obj;
                obj = DoRead(port, 0, 0, rif, fcf);
                return(MakePair(
                        rif ? MakeIdentifier(sym, GetLocation(port)) : sym,
                        MakePair(obj, EmptyListObject)));
            }

            default:
                if (SymbolCharP(ch))
                    return(DoReadSymbolOrNumber(port, ch, rif, fcf));
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

static FObject DoReadList(FObject port, int rif, int fcf)
{
    FObject obj;

    obj = DoRead(port, 0, 1, rif, fcf);

    if (obj == EolObject)
        return(EmptyListObject);

    if (obj == DotObject)
    {
        obj = DoRead(port, 0, 0, rif, fcf);

        if (DoRead(port, 0, 1, rif, fcf) != EolObject)
            RaiseExceptionC(R.Lexical, "read", "bad dotted pair", List(port));
        return(obj);
    }

    return(MakePair(obj, DoReadList(port, rif, fcf)));
}

FObject Read(FObject port, int rif, int fcf)
{
//    FAssert(OldInputPortP(port) && AsPort(port)->Context != 0);

    return(DoRead(port, 1, 0, rif, fcf));
}

// ---- Input ----

Define("read", ReadPrimitive)(int argc, FObject argv[])
{
    ZeroOrOneArgsCheck("read", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("read", port);

    return(Read(port, 0, 0));
}

Define("read-char", ReadCharPrimitive)(int argc, FObject argv[])
{
    FCh ch;

    ZeroOrOneArgsCheck("read-char", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("read-char", port);

    return(ReadCh(port, &ch) == 0 ? EndOfFileObject : MakeCharacter(ch));
}

Define("peek-char", PeekCharPrimitive)(int argc, FObject argv[])
{
    FCh ch;

    ZeroOrOneArgsCheck("peek-char", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("peek-char", port);

    return(PeekCh(port, &ch) == 0 ? EndOfFileObject : MakeCharacter(ch));
}

Define("read-line", ReadLinePrimitive)(int argc, FObject argv[])
{
    ZeroOrOneArgsCheck("read-line", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("read-line", port);

    return(ReadLine(port));
}

Define("eof-object?", EofObjectPPrimitive)(int argc, FObject argv[])
{
    OneArgCheck("eof-object?", argc);

    return(argv[0] == EndOfFileObject ? TrueObject : FalseObject);
}

Define("eof-object", EofObjectPrimitive)(int argc, FObject argv[])
{
    ZeroArgsCheck("eof-object", argc);

    return(EndOfFileObject);
}

Define("char-ready?", CharReadyPPrimitive)(int argc, FObject argv[])
{
    ZeroOrOneArgsCheck("char-ready?", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    TextualInputPortArgCheck("char-ready?", port);

    return(CharReadyP(port) ? TrueObject : FalseObject);
}

Define("read-string", ReadStringPrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("read-string", argc);
    NonNegativeArgCheck("read-string", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentInputPort());
    TextualInputPortArgCheck("read-string", port);

    return(ReadString(port, AsFixnum(argv[0])));
}

Define("read-u8", ReadU8Primitive)(int argc, FObject argv[])
{
    ZeroOrOneArgsCheck("read-u8", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    BinaryInputPortArgCheck("read-u8", port);

    FByte b;
    return(ReadBytes(port, &b, 1) == 0 ? EndOfFileObject : MakeFixnum(b));
}

Define("peek-u8", PeekU8Primitive)(int argc, FObject argv[])
{
    ZeroOrOneArgsCheck("peek-u8", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    BinaryInputPortArgCheck("peek-u8", port);

    FByte b;
    return(PeekByte(port, &b) == 0 ? EndOfFileObject : MakeFixnum(b));
}

Define("u8-ready?", U8ReadyPPrimitive)(int argc, FObject argv[])
{
    ZeroOrOneArgsCheck("u8-ready?", argc);
    FObject port = (argc == 1 ? argv[0] : CurrentInputPort());
    BinaryInputPortArgCheck("u8-ready?", port);

    return(ByteReadyP(port) ? TrueObject : FalseObject);
}

Define("read-bytevector", ReadBytevectorPrimitive)(int argc, FObject argv[])
{
    OneOrTwoArgsCheck("read-bytevector", argc);
    NonNegativeArgCheck("read-bytevector", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentInputPort());
    BinaryInputPortArgCheck("read-bytevector", port);

    int bvl = AsFixnum(argv[0]);
    FObject bv = MakeBytevector(bvl);
    int rl = ReadBytes(port, AsBytevector(bv)->Vector, bvl);
    if (rl == 0)
        return(EmptyListObject);

    if (rl == bvl)
        return(bv);

    FObject nbv = MakeBytevector(rl);
    memcpy(AsBytevector(nbv)->Vector, AsBytevector(bv)->Vector, rl);
    return(nbv);
}

Define("read-bytevector!", ReadBytevectorModifyPrimitive)(int argc, FObject argv[])
{
    OneToFourArgsCheck("read-bytevector!", argc);
    BytevectorArgCheck("read-bytevector!", argv[0]);
    FObject port = argc > 1 ? argv[1] : CurrentInputPort();
    BinaryInputPortArgCheck("read-bytevector!", port);

    int strt;
    int end;
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
            end = (int) BytevectorLength(argv[0]);
    }
    else
    {
        strt = 0;
        end = (int) BytevectorLength(argv[0]);
    }

    int rl = ReadBytes(port, AsBytevector(argv[0])->Vector + strt, end - strt);
    if (rl == 0)
        return(EmptyListObject);
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
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
