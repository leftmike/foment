/*

Foment

*/

#include <random>
#include "foment.hpp"

#define AsRandomSource(obj) ((FRandomSource *) (obj))
#define RandomSourceP(obj) (ObjectTag(obj) == RandomSourceTag)

typedef struct
{
    std::mt19937_64 Engine;
} FRandomSource;

static FObject MakeRandomSource()
{
    std::mt19937_64 seed(RandomSeed);
    FRandomSource * rs = (FRandomSource *) MakeObject(RandomSourceTag, sizeof(FRandomSource), 0,
            "make-random-source");
    rs->Engine = seed;
    return(rs);
}

void WriteRandomSource(FWriteContext * wctx, FObject obj)
{
    FAssert(RandomSourceP(obj));

    wctx->WriteStringC("#<random-source: ");

    FCh s[16];
    long_t sl = FixnumAsString((long_t) &AsRandomSource(obj)->Engine, s, 16);
    wctx->WriteString(s, sl);
    wctx->WriteCh('>');
}

inline void RandomSourceArgCheck(const char * who, FObject arg)
{
    if (RandomSourceP(arg) == 0)
        RaiseExceptionC(Assertion, who, "expected a random source", List(arg));
}

Define("random-source?", RandomSourcePPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("random-source?", argc);

    return(RandomSourceP(argv[0]) ? TrueObject : FalseObject);
}

Define("make-random-source", MakeRandomSourcePrimitive)(long_t argc, FObject argv[])
{
    ZeroArgsCheck("make-random-source", argc);

    return(MakeRandomSource());
}

// (%random-integer rs n)
Define("%random-integer", RandomIntegerPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("%random-integer", argc);
    RandomSourceArgCheck("%random-integer", argv[0]);
    if (FixnumP(argv[1]) == 0 || AsFixnum(argv[1]) <= 0)
        RaiseExceptionC(Assertion, "%random-integer", "expected a positive fixnum", List(argv[1]));

    std::uniform_int_distribution<long_t> dist(0, AsFixnum(argv[1]) - 1);
    return(MakeFixnum(dist(AsRandomSource(argv[0])->Engine)));
}

// (%random-real rs)
Define("%random-real", RandomRealPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("%random-real", argc);
    RandomSourceArgCheck("%random-real", argv[0]);

    std::uniform_real_distribution<double64_t> dist(0.0, 1.0);
    return(MakeFlonum(dist(AsRandomSource(argv[0])->Engine)));
}

// (random-source-state-ref rs) -> state
Define("random-source-state-ref", RandomSourceStateRefPrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("random-source-state-ref", argc);
    RandomSourceArgCheck("random-source-state-ref", argv[0]);

    FObject rs = MakeRandomSource();
    AsRandomSource(rs)->Engine = AsRandomSource(argv[0])->Engine;
    return(rs);
}

// (random-source-state-set! rs state)
Define("random-source-state-set!", RandomSourceStateSetPrimitive)(long_t argc, FObject argv[])
{
    TwoArgsCheck("random-source-state-set!", argc);
    RandomSourceArgCheck("random-source-state-set!", argv[0]);
    RandomSourceArgCheck("random-source-state-set!", argv[1]);

    AsRandomSource(argv[0])->Engine = AsRandomSource(argv[1])->Engine;
    return(NoValueObject);
}

// (random-source-randomize! rs)
Define("random-source-randomize!", RandomSourceRandomizePrimitive)(long_t argc, FObject argv[])
{
    OneArgCheck("random-source-randomize!", argc);
    RandomSourceArgCheck("random-source-randomize!", argv[0]);

    std::random_device rd;
    std::mt19937_64 seed(rd());
    AsRandomSource(argv[0])->Engine = seed;
    return(NoValueObject);
}

// (random-source-pseudo-randomize! rs i j)
Define("random-source-pseudo-randomize!",
        RandomSourcePseudoRandomizePrimitive)(long_t argc, FObject argv[])
{
    ThreeArgsCheck("random-source-pseudo-randomize!", argc);
    RandomSourceArgCheck("random-source-pseudo-randomize!", argv[0]);
    NonNegativeArgCheck("random-source-pseudo-randomize!", argv[1], 0);
    NonNegativeArgCheck("random-source-pseudo-randomize!", argv[2], 0);

    uint64_t i = AsFixnum(argv[1]);
    uint64_t j = AsFixnum(argv[2]);
    std::mt19937_64 seed((i << 32) | (j & 0xFFFFFFFF));
    AsRandomSource(argv[0])->Engine = seed;
    return(NoValueObject);
}

static FObject Primitives[] =
{
    RandomSourcePPrimitive,
    MakeRandomSourcePrimitive,
    RandomIntegerPrimitive,
    RandomRealPrimitive,
    RandomSourceStateRefPrimitive,
    RandomSourceStateSetPrimitive,
    RandomSourceRandomizePrimitive,
    RandomSourcePseudoRandomizePrimitive,
};

void SetupRandom()
{
    for (ulong_t idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
