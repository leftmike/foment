/*

Foment

*/

#include <stdio.h>
#include "foment.hpp"
#include "compile.hpp"

// ---- Syntax Rules ----

typedef struct
{
    FRecord Record;
    FObject Literals;
    FObject Rules;
    FObject SyntacticEnv;
} FSyntaxRules;

#define AsSyntaxRules(obj) ((FSyntaxRules *) (obj))

FObject SyntaxRulesRecordType;
static char * SyntaxRulesFieldsC[] = {"literals", "rules", "syntactic-env"};

static FObject MakeSyntaxRules(FObject lits, FObject rules, FObject se)
{
    FAssert(sizeof(FSyntaxRules) == sizeof(SyntaxRulesFieldsC) + sizeof(FRecord));

    FSyntaxRules * sr = (FSyntaxRules *) MakeRecord(SyntaxRulesRecordType);
    sr->Literals = lits;
    sr->Rules = rules;
    sr->SyntacticEnv = se;

    return(AsObject(sr));
}

// ---- Pattern Variable ----

typedef struct
{
    FRecord Record;
    FObject RepeatDepth;
    FObject Index;
    FObject Variable;
} FPatternVariable;

#define AsPatternVariable(obj) ((FPatternVariable *) (obj))
#define PatternVariableP(obj) RecordP(obj, PatternVariableRecordType)

static FObject PatternVariableRecordType;
static char * PatternVariableFieldsC[] = {"repeat-depth", "index", "variable"};

static FObject MakePatternVariable(int rd, FObject var)
{
    FAssert(sizeof(FPatternVariable) == sizeof(PatternVariableFieldsC) + sizeof(FRecord));
    FAssert(SymbolP(var));

    FPatternVariable * pv = (FPatternVariable *) MakeRecord(PatternVariableRecordType);
    pv->RepeatDepth = MakeFixnum(rd);
    pv->Index = MakeFixnum(-1);
    pv->Variable = var;

    return(AsObject(pv));
}

// ---- Pattern Repeat ----

typedef struct
{
    FRecord Record;
    FObject LeaveCount;
    FObject Ellipsis;
    FObject Variables;
    FObject Pattern;
    FObject Rest;
} FPatternRepeat;

#define AsPatternRepeat(obj) ((FPatternRepeat *) (obj))
#define PatternRepeatP(obj) RecordP(obj, PatternRepeatRecordType)

static FObject PatternRepeatRecordType;
static char * PatternRepeatFieldsC[] = {"leave-count", "ellipsis", "variables", "pattern", "rest"};

static FObject MakePatternRepeat(int lc, FObject ellip, FObject vars, FObject pat,
    FObject rest)
{
    FAssert(sizeof(FPatternRepeat) == sizeof(PatternRepeatFieldsC) + sizeof(FRecord));

    FPatternRepeat * pr = (FPatternRepeat *) MakeRecord(PatternRepeatRecordType);
    pr->LeaveCount = MakeFixnum(lc);
    pr->Ellipsis = ellip;
    pr->Variables = vars;
    pr->Pattern = pat;
    pr->Rest = rest;

    return(AsObject(pr));
}

// ---- Template Repeat ----

typedef struct
{
    FRecord Record;
    FObject Ellipsis;
    FObject RepeatCount;
    FObject Variables;
    FObject Template;
    FObject Rest;
} FTemplateRepeat;

#define AsTemplateRepeat(obj) ((FTemplateRepeat *) (obj))
#define TemplateRepeatP(obj) RecordP(obj, TemplateRepeatRecordType)

static FObject TemplateRepeatRecordType;
static char * TemplateRepeatFieldsC[] = {"ellipsis", "repeat-count", "variables", "template",
    "rest"};

static FObject MakeTemplateRepeat(FObject ellip, int rc)
{
    FAssert(sizeof(FTemplateRepeat) == sizeof(TemplateRepeatFieldsC) + sizeof(FRecord));

    FTemplateRepeat * tr = (FTemplateRepeat *) MakeRecord(TemplateRepeatRecordType);
    tr->Ellipsis = ellip;
    tr->RepeatCount = MakeFixnum(rc);
    tr->Variables = MakeVector(AsFixnum(tr->RepeatCount), 0, EmptyListObject);
    tr->Template = NoValueObject;
    tr->Rest = NoValueObject;

    return(AsObject(tr));
}

// ---- Syntax Rule ----

typedef struct
{
    FRecord Record;
    FObject NumVariables;
    FObject Variables; // A list of pattern variables.
    FObject Pattern;
    FObject Template;
} FSyntaxRule;

#define AsSyntaxRule(obj) ((FSyntaxRule *) (obj))
#define SyntaxRuleP(obj) RecordP(obj, SyntaxRuleRecordType)

static FObject SyntaxRuleRecordType;
static char * SyntaxRuleFieldsC[] = {"num-variables", "variables", "pattern", "template"};

static FObject MakeSyntaxRule(int nv, FObject vars, FObject pat, FObject tpl)
{
    FAssert(sizeof(FSyntaxRule) == sizeof(SyntaxRuleFieldsC) + sizeof(FRecord));

    FSyntaxRule * sr = (FSyntaxRule *) MakeRecord(SyntaxRuleRecordType);
    sr->NumVariables = MakeFixnum(nv);
    sr->Variables = vars;
    sr->Pattern = pat;
    sr->Template = tpl;

    return(AsObject(sr));
}

static FObject PatternToList(FObject cpat)
{
    if (PairP(cpat))
        return(MakePair(PatternToList(First(cpat)), PatternToList(Rest(cpat))));

    if (VectorP(cpat))
        return(ListToVector(AsVector(cpat)->Vector[0]));

    if (PatternRepeatP(cpat))
        return(MakePair(PatternToList(AsPatternRepeat(cpat)->Pattern),
                MakePair(AsPatternRepeat(cpat)->Ellipsis,
                PatternToList(AsPatternRepeat(cpat)->Rest))));

    return(cpat);
}

static FObject TemplateToList(FObject ctpl)
{
    if (PairP(ctpl))
        return(MakePair(TemplateToList(First(ctpl)), TemplateToList(Rest(ctpl))));

    if (VectorP(ctpl))
        return(ListToVector(AsVector(ctpl)->Vector[0]));

    if (TemplateRepeatP(ctpl))
    {
        FObject obj = TemplateToList(AsTemplateRepeat(ctpl)->Rest);
        for (int rc = 0; rc < AsFixnum(AsTemplateRepeat(ctpl)->RepeatCount); rc++)
            obj = MakePair(AsTemplateRepeat(ctpl)->Ellipsis, obj);

        return(MakePair(TemplateToList(AsTemplateRepeat(ctpl)->Template), obj));
    }

    return(ctpl);
}

// ----------------

static int LiteralFind(FObject list, FObject obj)
{
    while (list != EmptyListObject)
    {
        FAssert(PairP(list));
        FAssert(IdentifierP(First(list)));
        FAssert(IdentifierP(obj));

        if (AsIdentifier(First(list))->Symbol == AsIdentifier(obj)->Symbol)
            return(1);

        list = Rest(list);
    }

    return(0);
}

static FObject CopyLiterals(FObject obj, FObject ellip)
{
    FObject form = obj;
    FObject lits = EmptyListObject;

    while (obj != EmptyListObject)
    {
        if (PairP(obj) == 0 || IdentifierP(First(obj)) == 0)
            RaiseExceptionC(Syntax, "syntax-rules",
                    "syntax-rules: expected list of symbols for literals", List(form, obj));

        FObject id = First(obj);
        if (AsIdentifier(id)->Symbol == ellip || AsIdentifier(id)->Symbol == UnderscoreSymbol)
            RaiseExceptionC(Syntax, "syntax-rules",
                    "syntax-rules: <ellipsis> and _ not allowed as literals", List(form, id));

        if (LiteralFind(lits, id))
            RaiseExceptionC(Syntax, "syntax-rules", "syntax-rules: duplicate literal",
                    List(form, id));

        lits = MakePair(id, lits);
        obj = Rest(obj);
    }

    return(lits);
}

static FObject PatternVariableFind(FObject list, FObject var)
{
    FAssert(SymbolP(var));

    while (list != EmptyListObject)
    {
        FAssert(PairP(list));
        FAssert(PatternVariableP(First(list)));

        if (AsPatternVariable(First(list))->Variable == var)
            return(First(list));

        list = Rest(list);
    }

    return(NoValueObject);
}

static FObject CompilePatternVariables(FObject form, FObject lits, FObject pat,
    FObject ellip, FObject pvars, int rd)
{
    if (VectorP(pat))
        pat = VectorToList(pat);

    if (PairP(pat) && IdentifierP(First(pat)) && AsIdentifier(First(pat))->Symbol == ellip)
        RaiseExceptionC(Syntax, "syntax-rules",
                "syntax-rules: <ellipsis> must not start list pattern or vector pattern",
                List(form, pat));

    int ef = 0;
    while (PairP(pat))
    {
        if (PairP(Rest(pat)) && IdentifierP(First(Rest(pat)))
                && AsIdentifier(First(Rest(pat)))->Symbol == ellip)
        {
            if (ef)
                RaiseExceptionC(Syntax, "syntax-rules",
                        "syntax-rules: more than one <ellipsis> in a list pattern or vector pattern",
                        List(form, pat));

            ef = 1;
            pvars = CompilePatternVariables(form, lits, First(pat), ellip, pvars,
                    rd + 1);
            pat = Rest(Rest(pat));
        }
        else
        {
            pvars = CompilePatternVariables(form, lits, First(pat), ellip, pvars, rd);
            pat = Rest(pat);
        }
    }

    if (IdentifierP(pat))
    {
        if (AsIdentifier(pat)->Symbol != ellip && AsIdentifier(pat)->Symbol != UnderscoreSymbol)
        {
            if (PatternVariableP(PatternVariableFind(pvars, AsIdentifier(pat)->Symbol)))
                RaiseExceptionC(Syntax, "syntax-rules",
                        "syntax-rules: duplicate pattern variable", List(form, pat));

            if (LiteralFind(lits, pat) == 0)
                pvars = MakePair(MakePatternVariable(rd, AsIdentifier(pat)->Symbol), pvars);
        }
    }

    return(pvars);
}

static void AssignVariableIndexes(FObject pvars, int idx)
{
    while (pvars != EmptyListObject)
    {
        FAssert(PairP(pvars));
        FAssert(PatternVariableP(First(pvars)));

//        AsPatternVariable(First(pvars))->Index = MakeFixnum(idx);
        Modify(FPatternVariable, First(pvars), Index, MakeFixnum(idx));
        pvars = Rest(pvars);
        idx += 1;
    }
}

static int CountPatternsAfterRepeat(FObject pat)
{
    int n = 0;

    while (PairP(pat))
    {
        n += 1;
        pat = Rest(pat);
    }

    return(n);
}

static FObject RepeatPatternVariables(FObject pvars, FObject pat, FObject rvars)
{
    if (VectorP(pat))
        pat = VectorToList(pat);

    while (PairP(pat))
    {
        rvars = RepeatPatternVariables(pvars, First(pat), rvars);
        pat = Rest(pat);
    }

    if (IdentifierP(pat))
    {
        FObject var = PatternVariableFind(pvars, AsIdentifier(pat)->Symbol);
        if (PatternVariableP(var))
            return(MakePair(var, rvars));
    }

    return(rvars);
}

static FObject CompilePattern(FObject se, FObject lits, FObject pvars, FObject ellip, FObject pat)
{
    if (PairP(pat))
    {
        if (PairP(Rest(pat)) && IdentifierP(First(Rest(pat)))
                && AsIdentifier(First(Rest(pat)))->Symbol == ellip)
            return(MakePatternRepeat(CountPatternsAfterRepeat(Rest(Rest(pat))), ellip,
                    RepeatPatternVariables(pvars, First(pat), EmptyListObject),
                    CompilePattern(se, lits, pvars, ellip, First(pat)),
                    CompilePattern(se, lits, pvars, ellip, Rest(Rest(pat)))));

        return(MakePair(CompilePattern(se, lits, pvars, ellip, First(pat)),
                CompilePattern(se, lits, pvars, ellip, Rest(pat))));
    }
    else if (VectorP(pat))
        return(MakeVector(1, 0, CompilePattern(se, lits, pvars, ellip, VectorToList(pat))));
    else if (IdentifierP(pat))
    {
        if (AsIdentifier(pat)->Symbol == UnderscoreSymbol)
            return(UnderscoreSymbol);

        FObject var = PatternVariableFind(pvars, AsIdentifier(pat)->Symbol);
        if (PatternVariableP(var))
            return(var);
        FAssert(LiteralFind(lits, pat));

        return(MakeReference(ResolveIdentifier(se, pat), pat));
    }

    return(pat);
}

static int ListFind(FObject list, FObject obj)
{
    while (list != EmptyListObject)
    {
        FAssert(PairP(list));
        if (First(list) == obj)
            return(1);

        list = Rest(list);
    }

    return(0);
}

static int AddVarToTemplateRepeat(FObject var, FObject trs)
{
    int rd = AsFixnum(AsPatternVariable(var)->RepeatDepth);

    while (PairP(trs))
    {
        FObject tr = First(trs);
        FAssert(TemplateRepeatP(tr));

        for (int rc = 0; rc < AsFixnum(AsTemplateRepeat(tr)->RepeatCount); rc++)
        {
            if (ListFind(AsVector(AsTemplateRepeat(tr)->Variables)->Vector[rc], var)
                    == 0)
            {
//                AsVector(AsTemplateRepeat(tr)->Variables)->Vector[rc] =
//                        MakePair(var,
//                        AsVector(AsTemplateRepeat(tr)->Variables)->Vector[rc]);
                ModifyVector(AsTemplateRepeat(tr)->Variables, rc,
                        MakePair(var, AsVector(AsTemplateRepeat(tr)->Variables)->Vector[rc]));
            }

            rd -= 1;
            if (rd == 0)
                return(1);
        }

        trs = Rest(trs);
    }

    FAssert(trs == EmptyListObject);
    return(rd == 0);
}

static FObject CompileTemplate(FObject form, FObject pvars, FObject ellip, FObject tpl,
    FObject trs, int qea)
{
    if (PairP(tpl))
    {
        if (qea != 0 && IdentifierP(First(tpl)) && AsIdentifier(First(tpl))->Symbol == ellip)
        {
            if (PairP(Rest(tpl)) == 0 || Rest(Rest(tpl)) != EmptyListObject)
                RaiseExceptionC(Syntax, "syntax-rules",
                        "syntax-rules: expected (<ellipsis> <template>) or (... <template> <ellipsis> ...)",
                        List(form, tpl));

            return(CompileTemplate(form, pvars, NoValueObject, First(Rest(tpl)), trs,
                    0));
        }

        if (PairP(Rest(tpl)) && IdentifierP(First(Rest(tpl)))
                && AsIdentifier(First(Rest(tpl)))->Symbol == ellip)
        {
            FObject rpt = First(tpl);
            int rc = 0;

            tpl = Rest(tpl);
            while (PairP(tpl) && IdentifierP(First(tpl))
                    && AsIdentifier(First(tpl))->Symbol == ellip)
            {
                rc += 1;
                tpl = Rest(tpl);
            }

            FAssert(rc > 0);

            FObject tr = MakeTemplateRepeat(ellip, rc);
//            AsTemplateRepeat(tr)->Template = CompileTemplate(form, pvars, ellip, rpt,
//                    MakePair(tr, trs), 1);
            Modify(FTemplateRepeat, tr, Template, CompileTemplate(form, pvars, ellip, rpt,
                    MakePair(tr, trs), 1));

            for (int rc = 0; rc < AsFixnum(AsTemplateRepeat(tr)->RepeatCount); rc++)
                if (AsVector(AsTemplateRepeat(tr)->Variables)->Vector[rc]
                        == EmptyListObject)
                    RaiseExceptionC(Syntax, "syntax-rules",
                            "syntax-rules: missing repeated pattern variable for <ellipsis> in template",
                            List(form, tpl));

//            AsTemplateRepeat(tr)->Rest = CompileTemplate(form, pvars, ellip, tpl, trs,
//                    0);
            Modify(FTemplateRepeat, tr, Rest, CompileTemplate(form, pvars, ellip, tpl, trs, 0));

            return(tr);
        }

        return(MakePair(CompileTemplate(form, pvars, ellip, First(tpl), trs, 1),
                CompileTemplate(form, pvars, ellip, Rest(tpl), trs, 0)));
    }
    else if (VectorP(tpl))
        return(MakeVector(1, 0, CompileTemplate(form, pvars, ellip, VectorToList(tpl),
                trs, 0)));
    else if (IdentifierP(tpl))
    {
        FObject var = PatternVariableFind(pvars, AsIdentifier(tpl)->Symbol);
        if (PatternVariableP(var))
        {
            if (AsFixnum(AsPatternVariable(var)->RepeatDepth) > 0)
            {
                if (trs == EmptyListObject || AddVarToTemplateRepeat(var, trs) == 0)
                    RaiseExceptionC(Syntax, "syntax-rules",
                            "syntax-rules: missing <ellipsis> needed to repeat pattern variable in template",
                            List(form, tpl));
            }

            return(var);
        }

        return(tpl);
    }

    return(tpl);
}

#define MaxPatternVars 64

static FObject CompileRule(FObject se, FObject form, FObject lits, FObject rule, FObject ellip)
{
    // <syntax rule> is (<srpattern> <template>)

    if (PairP(rule) == 0 || PairP(Rest(rule)) == 0
            || Rest(Rest(rule)) != EmptyListObject)
        RaiseExceptionC(Syntax, "syntax-rules",
                "syntax-rules: expected (<pattern> <template>) for syntax rule", List(form, rule));

    FObject pat = First(rule);
    if (PairP(pat) == 0 || IdentifierP(First(pat)) == 0)
        RaiseExceptionC(Syntax, "syntax-rules",
                "syntax-rules: pattern must be list starting with a symbol", List(rule, pat));

    FObject pvars = ReverseListModify(CompilePatternVariables(pat, lits, Rest(pat), ellip,
            EmptyListObject, 0));
    AssignVariableIndexes(pvars, 0);

    if (ListLength(pvars) > MaxPatternVars)
        RaiseExceptionC(Restriction, "syntax-rules", "syntax-rules: too many pattern variables",
                List(rule));

    FObject cpat = CompilePattern(se, lits, pvars, ellip, Rest(pat));
    FObject tpl = CompileTemplate(First(Rest(rule)), pvars, ellip, First(Rest(rule)),
            EmptyListObject, 0);

    return(MakeSyntaxRule(ListLength(pvars), pvars, cpat, tpl));
}

FObject CompileSyntaxRules(FObject se, FObject obj)
{
    // (syntax-rules (<literal> ...) <syntax rule> ...)
    // (syntax-rules <ellipse> (<literal> ...) <syntax rule> ...)

    FObject form = obj;
    FObject ellip;

    FAssert(PairP(obj));

    if (PairP(Rest(obj)) && IdentifierP(First(Rest(obj))))
    {
        ellip = AsIdentifier(First(Rest(obj)))->Symbol;
        obj = Rest(obj);
    }
    else
        ellip = EllipsisSymbol;

    if (PairP(Rest(obj)) == 0)
        RaiseExceptionC(Syntax, "syntax-rules", "syntax-rules: missing literals", List(form));

    // (<literal> ...)

    FObject lits = ReverseListModify(CopyLiterals(First(Rest(obj)), ellip));

    FObject rules = Rest(Rest(obj));
    FObject nr = EmptyListObject;
    while (rules != EmptyListObject)
    {
        if (PairP(rules) == 0)
            RaiseExceptionC(Syntax, "syntax-rules", "syntax-rules: expected a list of rules",
                    List(form));

        if (PairP(First(rules)) == 0 || PairP(Rest(First(rules))) == 0
                || Rest(Rest(First(rules))) != EmptyListObject)
            RaiseExceptionC(Syntax, "syntax-rules",
                    "syntax-rules: expected (<pattern> <template>) for a rule",
                    List(form, First(rules)));

        nr = MakePair(CompileRule(se, form, lits, First(rules), ellip), nr);
        rules = Rest(rules);
    }

    nr = ReverseListModify(nr);
    return(MakeSyntaxRules(lits, nr, se));
}

// ----------------

static void InitRepeatVariables(FObject vars, FObject vals[], FObject rvals[])
{
    while (PairP(vars))
    {
        FAssert(PatternVariableP(First(vars)));

        int idx = AsFixnum(AsPatternVariable(First(vars))->Index);
        FAssert(idx >= 0 && idx < MaxPatternVars);

        vals[idx] = EmptyListObject;
        rvals[idx] = NoValueObject;

        vars = Rest(vars);
    }

    FAssert(vars == EmptyListObject);
}

static void GatherRepeatVariables(FObject vars, FObject vals[], FObject rvals[])
{
    while (PairP(vars))
    {
        FAssert(PatternVariableP(First(vars)));

        int idx = AsFixnum(AsPatternVariable(First(vars))->Index);
        FAssert(idx >= 0 && idx < MaxPatternVars);

        if (rvals[idx] != NoValueObject)
        {
            vals[idx] = MakePair(rvals[idx], vals[idx]);
            rvals[idx] = NoValueObject;
        }

        vars = Rest(vars);
    }

    FAssert(vars == EmptyListObject);
}

static int MatchPattern(FObject se, FObject cpat, FObject vals[], FObject expr)
{
    for (;;)
    {
        FAssert(IdentifierP(cpat) == 0);

        if (PairP(cpat))
        {
            if (PairP(expr) == 0)
                return(0);
            if (MatchPattern(se, First(cpat), vals, First(expr)) == 0)
                return(0);

            cpat = Rest(cpat);
            expr = Rest(expr);
        }
        else if (VectorP(cpat))
        {
            if (VectorP(expr) == 0)
                return(0);

            return(MatchPattern(se, AsVector(cpat)->Vector[0], vals, VectorToList(expr)));
        }
        else if (SymbolP(cpat))
        {
            FAssert(cpat == UnderscoreSymbol);
            return(1);
        }
        else if (ReferenceP(cpat))
            return(MatchReference(cpat, se, expr));
        else if (PatternVariableP(cpat))
        {
            vals[AsFixnum(AsPatternVariable(cpat)->Index)] = expr;
            return(1);
        }
        else if (PatternRepeatP(cpat))
        {
            int lc = AsFixnum(AsPatternRepeat(cpat)->LeaveCount);
            FObject obj = expr;
            while (lc > 0)
            {
                if (PairP(obj) == 0)
                    return(0);
                obj = Rest(obj);
                lc -= 1;
            }

            FObject rvals[MaxPatternVars];
            InitRepeatVariables(AsPatternRepeat(cpat)->Variables, vals, rvals);

            while (PairP(obj))
            {
                FAssert(PairP(expr));

                if (MatchPattern(se, AsPatternRepeat(cpat)->Pattern, rvals, First(expr))
                        == 0)
                    return(0);
                expr = Rest(expr);
                obj = Rest(obj);
                GatherRepeatVariables(AsPatternRepeat(cpat)->Variables, vals, rvals);
            }

            cpat = AsPatternRepeat(cpat)->Rest;
        }
        else
            return(EqualP(cpat, expr));
    }

    // Never reached.

    FAssert(0);
    return(0);
}

static int CheckRepeatVariables(FObject vars, FObject vals[], FObject expr)
{
    while (PairP(vars))
    {
        FAssert(PatternVariableP(First(vars)));

        int idx = AsFixnum(AsPatternVariable(First(vars))->Index);
        FAssert(idx >= 0 && idx < MaxPatternVars);

        if (vals[idx] == EmptyListObject)
            return(0);

        vars = Rest(vars);
    }

    FAssert(vars == EmptyListObject);
    return(1);
}

static void SetRepeatVariables(FObject vars, FObject vals[], FObject rvals[])
{
    while (PairP(vars))
    {
        FAssert(PatternVariableP(First(vars)));

        int idx = AsFixnum(AsPatternVariable(First(vars))->Index);
        FAssert(idx >= 0 && idx < MaxPatternVars);

        FAssert(PairP(vals[idx]));
        rvals[idx] = First(vals[idx]);
        vals[idx] = Rest(vals[idx]);

        vars = Rest(vars);
    }

    FAssert(vars == EmptyListObject);
}

static FObject ExpandTemplate(FObject tse, FObject use, FObject ctpl, int nv, FObject vals[],
    FObject expr);
static FObject ExpandTemplateRepeat(FObject tse, FObject use, FObject ctpl, int nv, FObject vals[],
    int rc, FObject ret, FObject expr)
{
    FAssert(TemplateRepeatP(ctpl));
    FAssert(rc > 0);

    FObject rvals[MaxPatternVars];
    for (int vdx = 0; vdx < nv; vdx++)
        rvals[vdx] = vals[vdx];

    rc -= 1;
    FObject vars = AsVector(AsTemplateRepeat(ctpl)->Variables)->Vector[rc];

    while (CheckRepeatVariables(vars, vals, expr))
    {
        SetRepeatVariables(vars, vals, rvals);
        if (rc == 0)
            ret = MakePair(ExpandTemplate(tse, use, AsTemplateRepeat(ctpl)->Template, nv, rvals,
                    expr), ret);
        else
            ret = ExpandTemplateRepeat(tse, use, ctpl, nv, rvals, rc, ret, expr);
    }

    while (PairP(vars))
    {
        FAssert(PatternVariableP(First(vars)));

        int idx = AsFixnum(AsPatternVariable(First(vars))->Index);
        FAssert(idx >= 0 && idx < MaxPatternVars);

        if (vals[idx] != EmptyListObject)
            RaiseExceptionC(Syntax, "syntax-rules",
                    "syntax-rules: <ellipsis> match counts differ in template expansion",
                    List(expr));

        vars = Rest(vars);
    }
    FAssert(vars == EmptyListObject);

    return(ret);
}

static FObject CopyWrapValue(FObject se, FObject val)
{
    if (IdentifierP(val))
        return(WrapIdentifier(val, se));

    if (PairP(val))
        return(MakePair(CopyWrapValue(se, First(val)), CopyWrapValue(se, Rest(val))));

    return(val);
}

static FObject ExpandTemplate(FObject tse, FObject use, FObject ctpl, int nv, FObject vals[],
    FObject expr)
{
    if (PairP(ctpl))
        return(MakePair(ExpandTemplate(tse, use, First(ctpl), nv, vals, expr),
                ExpandTemplate(tse, use, Rest(ctpl), nv, vals, expr)));

    if (VectorP(ctpl))
        return(ListToVector(ExpandTemplate(tse, use, AsVector(ctpl)->Vector[0], nv, vals, expr)));

    if (PatternVariableP(ctpl))
        return(CopyWrapValue(use, vals[AsFixnum(AsPatternVariable(ctpl)->Index)]));

    if (TemplateRepeatP(ctpl))
    {
        FObject rvals[MaxPatternVars];
        for (int vdx = 0; vdx < nv; vdx++)
            rvals[vdx] = vals[vdx];

        return(ExpandTemplateRepeat(tse, use, ctpl, nv, rvals,
                AsFixnum(AsTemplateRepeat(ctpl)->RepeatCount),
                ExpandTemplate(tse, use, AsTemplateRepeat(ctpl)->Rest, nv, vals, expr), expr));
    }

    if (IdentifierP(ctpl))
        return(WrapIdentifier(ctpl, tse));

    return(ctpl);
}

FObject ExpandSyntaxRules(FObject se, FObject sr, FObject expr)
{
    FAssert(SyntaxRulesP(sr));

    FObject rules = AsSyntaxRules(sr)->Rules;
    while (rules != EmptyListObject)
    {
        FObject rule = First(rules);
        FAssert(SyntaxRuleP(rule));
        FAssert(AsFixnum(AsSyntaxRule(rule)->NumVariables) <= MaxPatternVars);

        FObject vals[MaxPatternVars];
        for (int vdx = 0; vdx < AsFixnum(AsSyntaxRule(rule)->NumVariables); vdx++)
            vals[vdx] = NoValueObject;

        if (MatchPattern(se, AsSyntaxRule(rule)->Pattern, vals, expr))
            return(ExpandTemplate(MakeSyntacticEnv(AsSyntaxRules(sr)->SyntacticEnv), se,
                    AsSyntaxRule(rule)->Template, AsFixnum(AsSyntaxRule(rule)->NumVariables),
                    vals, expr));

        rules = Rest(rules);
    }

    RaiseExceptionC(Syntax, "syntax-rules", "syntax-rules: unable to match pattern",
            List(expr, sr));
    return(NoValueObject);
}

void SetupSyntaxRules()
{
    SyntaxRulesRecordType = MakeRecordTypeC("syntax-rules",
            sizeof(SyntaxRulesFieldsC) / sizeof(char *), SyntaxRulesFieldsC);
    PatternVariableRecordType = MakeRecordTypeC("pattern-variable",
            sizeof(PatternVariableFieldsC) / sizeof(char *), PatternVariableFieldsC);
    PatternRepeatRecordType = MakeRecordTypeC("pattern-repeat",
            sizeof(PatternRepeatFieldsC) / sizeof(char *), PatternRepeatFieldsC);
    TemplateRepeatRecordType = MakeRecordTypeC("template-repeat",
            sizeof(TemplateRepeatFieldsC) / sizeof(char *), TemplateRepeatFieldsC);
    SyntaxRuleRecordType = MakeRecordTypeC("syntax-rule",
            sizeof(SyntaxRuleFieldsC) / sizeof(char *), SyntaxRuleFieldsC);
}
