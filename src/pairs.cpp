/*

Foment

*/

#include "foment.hpp"

// ---- Pairs ----

FObject MakePair(FObject first, FObject rest)
{
    FPair * pair = (FPair *) MakeObject(PairTag, sizeof(FPair));
    pair->First = first;
    pair->Rest = rest;

    FObject obj = AsObject(pair);
    FAssert(ObjectLength(obj) == sizeof(FPair));
    return(obj);
}

int ListLength(FObject obj)
{
    int ll = 0;
    FObject list = obj;

    while (obj != EmptyListObject)
    {
        if (PairP(obj))
        {
            ll += 1;
            obj = Rest(obj);
        }
        else
            RaiseExceptionC(R.Assertion, "list-length", "list-length: not a proper list",
                    List(list));
    }

    return(ll);
}

FObject ReverseListModify(FObject list)
{
    FObject rlist = EmptyListObject;

    while (list != EmptyListObject)
    {
        FAssert(PairP(list));

        FObject obj = list;
        list = Rest(list);
//        AsPair(obj)->Rest = rlist;
        Modify(FPair, obj, Rest, rlist);
        rlist = obj;
    }

    return(rlist);
}

FObject List(FObject obj)
{
    return(MakePair(obj, EmptyListObject));
}

FObject List(FObject obj1, FObject obj2)
{
    return(MakePair(obj1, MakePair(obj2, EmptyListObject)));
}

FObject List(FObject obj1, FObject obj2, FObject obj3)
{
    return(MakePair(obj1, MakePair(obj2, MakePair(obj3, EmptyListObject))));
}

FObject List(FObject obj1, FObject obj2, FObject obj3, FObject obj4)
{
    return(MakePair(obj1, MakePair(obj2, MakePair(obj3, MakePair(obj4, EmptyListObject)))));
}

Define("pair?", PairPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "pair?", "pair?: expected one argument", EmptyListObject);

    return(PairP(argv[0]) ? TrueObject : FalseObject);
}

Define("cons", ConsPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "cons", "cons: expected two arguments", EmptyListObject);

    return(MakePair(argv[0], argv[1]));
}

Define("car", CarPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "car", "car: expected one argument", EmptyListObject);

    if (PairP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "car", "car: expected a pair", List(argv[0]));

    return(First(argv[0]));
}

Define("cdr", CdrPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "cdr", "cdr: expected one argument", EmptyListObject);

    if (PairP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "cdr", "cdr: expected a pair", List(argv[0]));

    return(Rest(argv[0]));
}

Define("set-car!", SetCarPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "set-car!", "set-car!: expected two arguments",
                EmptyListObject);

    if (PairP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "set-car!", "set-car!: expected a pair", List(argv[0]));

//    AsPair(argv[0])->First = argv[1];
    Modify(FPair, argv[0], First, argv[1]);

    return(NoValueObject);
}

Define("set-cdr!", SetCdrPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "set-cdr!", "set-cdr!: expected two arguments",
                EmptyListObject);

    if (PairP(argv[0]) == 0)
        RaiseExceptionC(R.Assertion, "set-cdr!", "set-cdr!: expected a pair", List(argv[0]));

//    AsPair(argv[0])->Rest = argv[1];
    Modify(FPair, argv[0], Rest, argv[1]);

    return(NoValueObject);
}

Define("list", ListPrimitive)(int argc, FObject argv[])
{
    FObject lst = EmptyListObject;
    int adx = argc;

    while (adx > 0)
    {
        adx -= 1;
        lst = MakePair(argv[adx], lst);
    }

    return(lst);
}

Define("null?", NullPPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "null?", "null?: expected one argument", EmptyListObject);

    return(argv[0] == EmptyListObject ? TrueObject : FalseObject);
}

static FObject ReverseList(FObject list)
{
    FObject rlst = EmptyListObject;
    FObject lst = list;

    while (lst != EmptyListObject)
    {
        if (PairP(lst) == 0)
            RaiseExceptionC(R.Assertion, "reverse", "reverse: expected a proper list", List(list));

        rlst = MakePair(First(lst), rlst);
        lst = Rest(lst);
    }

    return(rlst);
}

Define("append", AppendPrimitive)(int argc, FObject argv[])
{
    if (argc < 1)
        RaiseExceptionC(R.Assertion, "append", "append: expected at least one argument",
                EmptyListObject);

    FObject ret = argv[argc - 1];

    int adx = argc - 1;
    while (adx > 0)
    {
        adx -= 1;
        FObject lst = ReverseList(argv[adx]);

        while (lst != EmptyListObject)
        {
            FAssert(PairP(lst));

            FObject obj = lst;
            lst = Rest(lst);
//            AsPair(obj)->Rest = ret;
            Modify(FPair, obj, Rest, ret);
            ret = obj;
        }
  }

    return(ret);
}

Define("reverse", ReversePrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "reverse", "reverse: expected one argument", EmptyListObject);

    return(ReverseList(argv[0]));
}

Define("list-ref", ListRefPrimitive)(int argc, FObject argv[])
{
    if (argc != 2)
        RaiseExceptionC(R.Assertion, "list-ref", "list-ref: expected two arguments",
                EmptyListObject);

    if (FixnumP(argv[1]) == 0)
        RaiseExceptionC(R.Assertion, "list-ref", "list-ref: expected a fixnum", List(argv[1]));

    FObject lst = argv[0];
    int k = AsFixnum(argv[1]);

    while (k > 0)
    {
        if (PairP(lst) == 0)
            RaiseExceptionC(R.Assertion, "list-ref", "list-ref: expected a list", List(argv[0]));

        k -= 1;
        lst = Rest(lst);
    }

    if (PairP(lst) == 0)
        RaiseExceptionC(R.Assertion, "list-ref", "list-ref: expected a list", List(argv[0]));

    return(First(lst));
}

Define("map-car", MapCarPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "map-car", "map-car: expected one argument", EmptyListObject);

    FObject ret = EmptyListObject;
    FObject lst = argv[0];

    while (PairP(lst))
    {
        if (PairP(First(lst)) == 0)
            return(EmptyListObject);

        ret = MakePair(First(First(lst)), ret);
        lst = Rest(lst);
    }

    return(ReverseListModify(ret));
}

Define("map-cdr", MapCdrPrimitive)(int argc, FObject argv[])
{
    if (argc != 1)
        RaiseExceptionC(R.Assertion, "map-cdr", "map-cdr: expected one argument", EmptyListObject);

    FObject ret = EmptyListObject;
    FObject lst = argv[0];

    while (PairP(lst))
    {
        if (PairP(First(lst)) == 0)
            return(EmptyListObject);

        ret = MakePair(Rest(First(lst)), ret);
        lst = Rest(lst);
    }

    return(ReverseListModify(ret));
}

FObject Assq(FObject obj, FObject alst)
{
    while (PairP(alst))
    {
        FAssert(PairP(First(alst)));

        if (EqP(First(First(alst)), obj))
            return(First(alst));

        alst = Rest(alst);
    }

    FAssert(alst == EmptyListObject);

    return(FalseObject);
}

static FPrimitive * Primitives[] =
{
    &PairPPrimitive,
    &ConsPrimitive,
    &CarPrimitive,
    &CdrPrimitive,
    &SetCarPrimitive,
    &SetCdrPrimitive,
    &ListPrimitive,
    &NullPPrimitive,
    &AppendPrimitive,
    &ReversePrimitive,
    &ListRefPrimitive,
    &MapCarPrimitive,
    &MapCdrPrimitive
};

void SetupPairs()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
