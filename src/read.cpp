/*

Foment

*/

#include <windows.h>
#include "foment.hpp"
#include "syncthrd.hpp"
#include "io.hpp"
#include "unicode.hpp"

#define MAXIMUM_IDENTIFIER 256
#define MAXIMUM_NAME 32
#define MAXIMUM_NUMBER 32

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

static int_t IdentifierSubsequentP(FCh ch)
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

static FObject ReadStringLiteral(FObject port, FCh tch)
{
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

static FObject ReadNumber(FObject port, FCh * s, int_t sl, int_t sdx)
{
    FFixnum n;
    FCh ch;

    FAssert(sl == MAXIMUM_NUMBER);

    for (;;)
    {
        if (PeekCh(port, &ch) == 0)
            break;

        if (IdentifierSubsequentP(ch) == 0)
            break;

        s[sdx] = ch;
        sdx += 1;
        if (sdx == sl)
            RaiseExceptionC(R.Restriction, "read", "number too long", List(port));

        ReadCh(port, &ch);
    }

    if (StringToNumber(s, sdx, &n, 10) == 0)
        RaiseExceptionC(R.Lexical, "read", "expected a valid number",
                List(port, MakeString(s, sdx)));

    return(MakeFixnum(n));
}

static int_t ReadName(FObject port, FCh ch, FCh * s)
{
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

static FObject ReadIdentifier(FObject port, FCh * s, int_t sl, int_t sdx)
{
    int_t ln;
    FCh ch;

    FAssert(sl == MAXIMUM_IDENTIFIER);

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
        if (sdx == sl)
            RaiseExceptionC(R.Restriction, "read", "symbol too long", List(port));

        ReadCh(port, &ch);
    }

    FObject sym = FoldcasePortP(port) ? StringToSymbol(FoldcaseString(MakeString(s, sdx)))
            : StringLengthToSymbol(s, sdx);
    if (WantIdentifiersPortP(port))
        return(MakeIdentifier(sym, ln));
    return(sym);
}

static FObject ReadList(FObject port);
static FObject Read(FObject port, int_t eaf, int_t rlf);
static FObject ReadSharp(FObject port, int_t eaf, int_t rlf)
{
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
        FCh s[MAXIMUM_NUMBER];
        s[0] = '#';
        s[1] = ch;
        return(ReadNumber(port, s, sizeof(s) / sizeof(FCh), 2));
    }
    else if (ch ==  '(')
        return(ListToVector(ReadList(port)));
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
        return(U8ListToBytevector(ReadList(port)));
    }
    else if (ch == ';')
    {
        Read(port, 0, 0);

        return(Read(port, eaf, rlf));
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

        return(Read(port, eaf, rlf));
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

        return(Read(port, eaf, rlf));
    }

    RaiseExceptionC(R.Lexical, "read", "unexpected character following #",
            List(port, MakeCharacter(ch)));

    return(NoValueObject);
}

static FObject Read(FObject port, int_t eaf, int_t rlf)
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
                return(ReadSharp(port, eaf, rlf));

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
                return(ReadList(port));

            case ')':
                if (rlf)
                    return(EolObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected )", List(port));
                break;

            case '.':
                if (PeekCh(port, &ch) == 0)
                    RaiseExceptionC(R.Lexical, "read",
                            "unexpected end-of-file reading dotted pair", List(port));

                if (DotSubsequentP(ch))
                {
                    FCh s[MAXIMUM_IDENTIFIER];
                    s[0] = '.';
                    return(ReadIdentifier(port, s, sizeof(s) / sizeof(FCh), 1));
                }
                else if (DelimiterP(ch) == 0)
                {
                    FCh s[MAXIMUM_NUMBER];
                    s[0] = '.';
                    return(ReadNumber(port, s, sizeof(s) / sizeof(FCh), 1));
                }

                if (rlf)
                    return(DotObject);
                RaiseExceptionC(R.Lexical, "read", "unexpected dotted pair", List(port));
                break;

            case '\'':
            {
                FObject obj;
                obj = Read(port, 0, 0);
                return(MakePair(WantIdentifiersPortP(port) ? MakeIdentifier(R.QuoteSymbol,
                        GetLineColumn(port, 0)) : R.QuoteSymbol, MakePair(obj, EmptyListObject)));
            }

            case '`':
            {
                FObject obj;
                obj = Read(port, 0, 0);
                return(MakePair(WantIdentifiersPortP(port)
                        ? MakeIdentifier(R.QuasiquoteSymbol, GetLineColumn(port, 0))
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
                obj = Read(port, 0, 0);
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
                    return(ReadIdentifier(port, s, sizeof(s) / sizeof(FCh), 1));
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
                        return(ReadIdentifier(port, s, sizeof(s) / sizeof(FCh), 2));
                    }
                    else
                    {
                        FCh s[MAXIMUM_NUMBER];
                        s[0] = ch;
                        s[1] = ch2;
                        return(ReadNumber(port, s, sizeof(s) / sizeof(FCh), 2));
                    }
                }
                else
                {
                    FCh s[MAXIMUM_NUMBER];
                    s[0] = ch;
                    return(ReadNumber(port, s, sizeof(s) / sizeof(FCh), 1));
                }
            }

            default:
                if (IdentifierInitialP(ch))
                {
                    FCh s[MAXIMUM_IDENTIFIER];
                    s[0] = ch;
                    return(ReadIdentifier(port, s, sizeof(s) / sizeof(FCh), 1));
                }
                else if (NumericP(ch))
                {
                    FCh s[MAXIMUM_NUMBER];
                    s[0] = ch;
                    return(ReadNumber(port, s, sizeof(s) / sizeof(FCh), 1));
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

static FObject ReadList(FObject port)
{
    FObject obj;

    obj = Read(port, 0, 1);

    if (obj == EolObject)
        return(EmptyListObject);

    if (obj == DotObject)
    {
        obj = Read(port, 0, 0);

        if (Read(port, 0, 1) != EolObject)
            RaiseExceptionC(R.Lexical, "read", "bad dotted pair", List(port));
        return(obj);
    }

    return(MakePair(obj, ReadList(port)));
}

FObject Read(FObject port)
{
    FAssert(InputPortP(port) && InputPortOpenP(port));

    return(Read(port, 1, 0));
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
    NonNegativeArgCheck("read-string", argv[0]);
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
    NonNegativeArgCheck("read-bytevector", argv[0]);
    FObject port = (argc == 2 ? argv[1] : CurrentInputPort());
    BinaryInputPortArgCheck("read-bytevector", port);

    int_t bvl = AsFixnum(argv[0]);
    FObject bv = MakeBytevector(bvl);
    int_t rl = ReadBytes(port, AsBytevector(bv)->Vector, bvl);
    if (rl == 0)
        return(EmptyListObject);

    if (rl == bvl)
        return(bv);

    FObject nbv = MakeBytevector(rl);
    memcpy(AsBytevector(nbv)->Vector, AsBytevector(bv)->Vector, rl);
    return(nbv);
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

    int_t rl = ReadBytes(port, AsBytevector(argv[0])->Vector + strt, end - strt);
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
    for (int_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
