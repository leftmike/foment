/*

Foment

*/

#include "foment.hpp"

static inline int Height(FObject tree)
{
    FAssert(AVLTreeP(tree) == 0 || FixnumP(AsAVLTree(tree)->Height));

    return(AVLTreeP(tree) ? AsFixnum(AsAVLTree(tree)->Height) : 0);
}

static char * AVLTreeFields[] = {"key", "value", "height", "left", "right"};

static FObject MakeAVLTree(FObject key, FObject val, FObject lft, FObject rt)
{
    FAssert(sizeof(FAVLTree) == sizeof(AVLTreeFields) + sizeof(FRecord));

    FAVLTree * tree = (FAVLTree *) MakeRecord(R.AVLTreeRecordType);
    tree->Key = key;
    tree->Value = val;

    int rh = Height(rt);
    int lh = Height(lft);
    tree->Height = MakeFixnum(lh > rh ? lh + 1 : rh + 1);

    tree->Left = lft;
    tree->Right = rt;

    return(tree);
}

static FObject MakeAVLTreeLeft(FObject tree, FObject lft)
{
    FAssert(AVLTreeP(tree));

    return(MakeAVLTree(AsAVLTree(tree)->Key, AsAVLTree(tree)->Value, lft, AsAVLTree(tree)->Right));
}

static FObject MakeAVLTreeRight(FObject tree, FObject rt)
{
    FAssert(AVLTreeP(tree));

    return(MakeAVLTree(AsAVLTree(tree)->Key, AsAVLTree(tree)->Value, AsAVLTree(tree)->Left, rt));
}

FObject AVLTreeRef(FObject tree, FObject key, FObject def, FCompareFn cfn)
{
    while (AVLTreeP(tree))
    {
        FAssert((Height(tree) == Height(AsAVLTree(tree)->Left) + 1
                && Height(AsAVLTree(tree)->Left) >= Height(AsAVLTree(tree)->Right))
                || (Height(tree) == Height(AsAVLTree(tree)->Right) + 1
                && Height(AsAVLTree(tree)->Right) >= Height(AsAVLTree(tree)->Left)));

        int cmp = cfn(AsAVLTree(tree)->Key, key);
        if (cmp == 0)
            return(AsAVLTree(tree)->Value);

        if (cmp > 0)
            tree = AsAVLTree(tree)->Right;
        else
            tree = AsAVLTree(tree)->Left;
    }

    FAssert(tree == EmptyListObject);

    return(def);
}

static FObject MakeBalanced(FObject tree)
{
    FAssert(AVLTreeP(tree));
    
    
    
    return(tree);
}

FObject AVLTreeSet(FObject tree, FObject key, FObject val, FCompareFn cfn, int af)
{
    FAssert(tree == EmptyListObject || AVLTreeP(tree));

    if (AVLTreeP(tree) == 0)
        return(MakeAVLTree(key, af ? MakePair(val, EmptyListObject) : val , EmptyListObject,
                EmptyListObject));

    int cmp = cfn(AsAVLTree(tree)->Key, key);
    if (cmp == 0)
        return(MakeAVLTree(key, af ? MakePair(val, AsAVLTree(tree)->Value) : val,
                AsAVLTree(tree)->Left, AsAVLTree(tree)->Right));

    if (cmp > 0)
        return(MakeAVLTreeRight(tree, AVLTreeSet(AsAVLTree(tree)->Right, key, val, cfn, af)));

    return(MakeAVLTreeLeft(tree, AVLTreeSet(AsAVLTree(tree)->Left, key, val, cfn, af)));
}

FObject AVLTreeDelete(FObject tree, FObject key, FCompareFn cfn)
{
    FAssert(0);

    FAssert(tree == EmptyListObject || AVLTreeP(tree));
    
    
    
    return(tree);
}

static void AVLTreeWrite(FObject tree)
{
    FAssert(AVLTreeP(tree));

    FAssert((Height(tree) == Height(AsAVLTree(tree)->Left) + 1
            && Height(AsAVLTree(tree)->Left) >= Height(AsAVLTree(tree)->Right))
            || (Height(tree) == Height(AsAVLTree(tree)->Right) + 1
            && Height(AsAVLTree(tree)->Right) >= Height(AsAVLTree(tree)->Left)));

    if (AVLTreeP(AsAVLTree(tree)->Left))
        AVLTreeWrite(AsAVLTree(tree)->Left);

    FAssert(FixnumP(AsAVLTree(tree)->Height));

    PutCh(R.StandardOutput, '\n');
    for (int sdx = 0; sdx < AsFixnum(AsAVLTree(tree)->Height); sdx++)
        PutCh(R.StandardOutput, ' ');
    PutCh(R.StandardOutput, '[');
    WriteShared(R.StandardOutput, AsAVLTree(tree)->Height, 0);
    PutStringC(R.StandardOutput, "] ");
    WriteShared(R.StandardOutput, AsAVLTree(tree)->Key, 0);
    PutStringC(R.StandardOutput, ": ");
    WriteShared(R.StandardOutput, AsAVLTree(tree)->Value, 0);

    if (AVLTreeP(AsAVLTree(tree)->Right))
        AVLTreeWrite(AsAVLTree(tree)->Right);
}

static inline int ComparableP(FObject obj)
{
    return(FixnumP(obj) || CharacterP(obj) || StringP(obj) || SymbolP(obj));
}

static int Compare(FObject obj1, FObject obj2)
{
    FAssert(ComparableP(obj1));
    FAssert(ComparableP(obj2));

    if (FixnumP(obj1) && FixnumP(obj2))
        return(AsFixnum(obj1) - AsFixnum(obj2));

    if (CharacterP(obj1) && CharacterP(obj2))
        return(AsCharacter(obj1) - AsCharacter(obj2));

    if (StringP(obj1) && StringP(obj2))
        return(StringCompare(AsString(obj1), AsString(obj2)));

    if (SymbolP(obj1) && SymbolP(obj2))
        return(Compare(AsSymbol(obj1)->String, AsSymbol(obj2)->String));

    if (IndirectP(obj1))
        return(IndirectP(obj2) ? IndirectTag(obj1) - IndirectTag(obj2) : -1);

    if (IndirectP(obj2))
        return(1);

    return(FixnumP(obj1) ? 1 : -1);
}

Define("avl-tree-ref", AVLTreeRefPrimitive)(int argc, FObject argv[])
{
    // (avl-tree-ref <tree> <key> <default>)

    if (argc != 3)
        RaiseExceptionC(R.Assertion, "avl-tree-ref",
                "avl-tree-ref: expected three arguments", EmptyListObject);

    if (argv[0] != EmptyListObject && AVLTreeP(argv[0]) == 0)
         RaiseExceptionC(R.Assertion, "avl-tree-ref",
                 "avl-tree-ref: expected an AVL tree", List(argv[0]));

    if (ComparableP(argv[1]) == 0)
         RaiseExceptionC(R.Assertion, "avl-tree-ref",
                 "avl-tree-ref: expected a value that can be compared", List(argv[1]));

    return(AVLTreeRef(argv[0], argv[1], argv[2], Compare));
}

Define("avl-tree-set", AVLTreeSetPrimitive)(int argc, FObject argv[])
{
    // (avl-tree-set <tree> <key> <value>)

    if (argc != 3)
        RaiseExceptionC(R.Assertion, "avl-tree-set",
                "avl-tree-set: expected three arguments", EmptyListObject);

    if (argv[0] != EmptyListObject && AVLTreeP(argv[0]) == 0)
         RaiseExceptionC(R.Assertion, "avl-tree-set",
                 "avl-tree-set: expected an AVL tree", List(argv[0]));

    if (ComparableP(argv[1]) == 0)
         RaiseExceptionC(R.Assertion, "avl-tree-set",
                 "avl-tree-set: expected a value that can be compared", List(argv[1]));

    return(AVLTreeSet(argv[0], argv[1], argv[2], Compare, 0));
}

Define("avl-tree-grow", AVLTreeGrowPrimitive)(int argc, FObject argv[])
{
    // (avl-tree-grow <tree> <key> <value>)

    if (argc != 3)
        RaiseExceptionC(R.Assertion, "avl-tree-grow",
                "avl-tree-grow: expected three arguments", EmptyListObject);

    if (argv[0] != EmptyListObject && AVLTreeP(argv[0]) == 0)
         RaiseExceptionC(R.Assertion, "avl-tree-grow",
                 "avl-tree-grow: expected an AVL tree", List(argv[0]));

    if (ComparableP(argv[1]) == 0)
         RaiseExceptionC(R.Assertion, "avl-tree-grow",
                 "avl-tree-grow: expected a value that can be compared", List(argv[1]));

    return(AVLTreeSet(argv[0], argv[1], argv[2], Compare, 1));
}

Define("avl-tree-write", AVLTreeWritePrimitive)(int argc, FObject argv[])
{
    // (avl-tree-write <tree>)

    if (argc != 1)
        RaiseExceptionC(R.Assertion, "avl-tree-write",
                "avl-tree-write: expected one argument", EmptyListObject);

    if (AVLTreeP(argv[0]) == 0)
         RaiseExceptionC(R.Assertion, "avl-tree-write",
                 "avl-tree-write: expected an AVL tree", List(argv[0]));

    AVLTreeWrite(argv[0]);
    return(NoValueObject);
}

static FPrimitive * Primitives[] =
{
    &AVLTreeRefPrimitive,
    &AVLTreeSetPrimitive,
    &AVLTreeGrowPrimitive,
    &AVLTreeWritePrimitive
};

void SetupAVLTrees()
{
    R.AVLTreeRecordType = MakeRecordTypeC("avl-tree",
            sizeof(AVLTreeFields) / sizeof(char *), AVLTreeFields);

    for (int idx = 0; idx < sizeof(Primitives) / sizeof(FPrimitive *); idx++)
        DefinePrimitive(R.Bedrock, R.BedrockLibrary, Primitives[idx]);
}
