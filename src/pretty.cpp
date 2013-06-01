/*

Foment

*/

#include "foment.hpp"
#include "io.hpp"
#include "compile.hpp"

static void WritePrettyObject(FObject port, FObject obj, int df, FWriteFn wfn, void * ctx)
{
    if (EnvironmentP(obj))
    {
        PutStringC(port, "#<");
        WriteGeneric(port, AsEnvironment(obj)->Name, 1, (FWriteFn) WriteGeneric, 0);
        PutCh(port, '>');
    }
    else if (LibraryP(obj))
    {
        PutStringC(port, "#<library: ");
        WriteGeneric(port, AsLibrary(obj)->Name, 1, (FWriteFn) WriteGeneric, 0);
        PutStringC(port, " (");

        FObject lst = AsLibrary(obj)->Exports;
        while (PairP(lst))
        {
            FAssert(PairP(First(lst)));
            FAssert(SymbolP(First(First(lst))));

            WriteGeneric(port, First(First(lst)), 1, (FWriteFn) WriteGeneric, 0);
            lst = Rest(lst);

            if (lst != EmptyListObject)
                PutCh(port, ' ');
        }

        PutStringC(port, ")>");
    }
    else if (IdentifierP(obj))
    {
        WriteGeneric(port, AsIdentifier(obj)->Symbol, 1, (FWriteFn) WriteGeneric, 0);
        PutCh(port, '.');
        WriteGeneric(port, AsIdentifier(obj)->Magic, 1, (FWriteFn) WriteGeneric, 0);
    }
    else if (ReferenceP(obj))
    {
        wfn(port, AsReference(obj)->Identifier, 1, wfn, ctx);
        PutCh(port, ':');
        wfn(port, AsReference(obj)->Binding, 1, wfn, ctx);
    }
    else if (BindingP(obj))
    {
        FCh s[16];
        int sl = NumberAsString((FFixnum) obj, s, 16);

        PutStringC(port, "#<binding: #x");
        PutString(port, s, sl);
        PutCh(port, ' ');
        wfn(port, AsBinding(obj)->Identifier, 1, wfn, ctx);
        PutCh(port, '>');
    }
    else if (LambdaP(obj))
    {
        PutStringC(port, "#<(lambda: ");
        if (AsLambda(obj)->Name != NoValueObject)
        {
            wfn(port, AsLambda(obj)->Name, 1, wfn, ctx);
            PutCh(port, ' ');
        }

        wfn(port, AsLambda(obj)->Bindings, 1, wfn, ctx);

        PutCh(port, ' ');
        wfn(port, AsLambda(obj)->Body, 1, wfn, ctx);
        PutStringC(port, ")>");
    }
    else if (ctx == 0)
        WriteGeneric(port, obj, df, wfn, 0);
    else
        WriteSharedObject(port, obj, df, wfn, ctx);
}

void WritePretty(FObject port, FObject obj, int df)
{
    FObject ht = MakeHashtable(23);

    if (SharedObjectP(obj))
    {
        if (FindSharedObjects(ht, obj, 0, 1) == 0)
        {
            FAssert(HashtableSize(ht) == 0);

            WriteGeneric(port, obj, df, (FWriteFn) WritePrettyObject, 0);
        }
        else
        {
            HashtableWalkUpdate(ht, WalkUpdate, NoValueObject);

            FWriteSharedCtx ctx;
            ctx.Hashtable = ht;
            ctx.Label = -1;
            WritePrettyObject(port, obj, df, (FWriteFn) WritePrettyObject, &ctx);
        }
    }
    else
        WriteGeneric(port, obj, df, (FWriteFn) WriteGeneric, 0);
}

// ---- Primitives ----

Define("write-pretty", WritePrettyPrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, "write-pretty", "write-pretty: expected one or two arguments",
                EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(Assertion, "write-pretty",
                    "write-pretty: expected an output port", List(argv[1]));

        port = argv[1];
    }
    else
        port = StandardOutput;

    WritePretty(port, argv[0], 0);

    return(NoValueObject);
}

Define("display-pretty", DisplayPrettyPrimitive)(int argc, FObject argv[])
{
    FObject port;

    if (argc < 1 || argc > 2)
        RaiseExceptionC(Assertion, "display-pretty",
                "display-pretty: expected one or two arguments", EmptyListObject);

    if (argc == 2)
    {
        if (OutputPortP(argv[1]) == 0)
            RaiseExceptionC(Assertion, "display-pretty",
                    "display-pretty: expected an output port", List(argv[1]));

        port = argv[1];
    }
    else
        port = StandardOutput;

    WritePretty(port, argv[0], 1);

    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &WritePrettyPrimitive,
    &DisplayPrettyPrimitive
};

void SetupPrettyPrint()
{
    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(Bedrock, BedrockLibrary, Primitives[idx]);
}
