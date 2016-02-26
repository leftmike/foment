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

static const char * SyntaxRulesFieldsC[] = {"literals", "rules", "syntactic-env"};

static FObject MakeSyntaxRules(FObject lits, FObject rules, FObject se)
{
    FAssert(sizeof(FSyntaxRules) == sizeof(SyntaxRulesFieldsC) + sizeof(FRecord));

    FSyntaxRules * sr = (FSyntaxRules *) MakeRecord(R.SyntaxRulesRecordType);
    sr->Literals = lits;
    sr->Rules = rules;
    sr->SyntacticEnv = se;

    return(sr);
}

// ---- Pattern Variable ----

static const char * PatternVariableFieldsC[] = {"repeat-depth", "index", "variable"};

static FObject MakePatternVariable(int_t rd, FObject var)
{
    FAssert(sizeof(FPatternVariable) == sizeof(PatternVariableFieldsC) + sizeof(FRecord));
    FAssert(ReferenceP(var));

    FPatternVariable * pv = (FPatternVariable *) MakeRecord(R.PatternVariableRecordType);
    pv->RepeatDepth = MakeFixnum(rd);
    pv->Index = MakeFixnum(-1);
    pv->Variable = var;

    return(pv);
}

// ---- Pattern Repeat ----

static const char * PatternRepeatFieldsC[] = {"leave-count", "ellipsis", "variables", "pattern",
    "rest"};

static FObject MakePatternRepeat(int_t lc, FObject ellip, FObject vars, FObject pat,
    FObject rest)
{
    FAssert(sizeof(FPatternRepeat) == sizeof(PatternRepeatFieldsC) + sizeof(FRecord));

    FPatternRepeat * pr = (FPatternRepeat *) MakeRecord(R.PatternRepeatRecordType);
    pr->LeaveCount = MakeFixnum(lc);
    pr->Ellipsis = ellip;
    pr->Variables = vars;
    pr->Pattern = pat;
    pr->Rest = rest;

    return(pr);
}

// ---- Template Repeat ----

static const char * TemplateRepeatFieldsC[] = {"ellipsis", "repeat-count", "variables", "template",
    "rest"};

static FObject MakeTemplateRepeat(FObject ellip, int_t rc)
{
    FAssert(sizeof(FTemplateRepeat) == sizeof(TemplateRepeatFieldsC) + sizeof(FRecord));

    FTemplateRepeat * tr = (FTemplateRepeat *) MakeRecord(R.TemplateRepeatRecordType);
    tr->Ellipsis = ellip;
    tr->RepeatCount = MakeFixnum(rc);
    tr->Variables = MakeVector(AsFixnum(tr->RepeatCount), 0, EmptyListObject);
    tr->Template = NoValueObject;
    tr->Rest = NoValueObject;

    return(tr);
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
#define SyntaxRuleP(obj) RecordP(obj, R.SyntaxRuleRecordType)

static const char * SyntaxRuleFieldsC[] = {"num-variables", "variables", "pattern", "template"};

static FObject MakeSyntaxRule(int_t nv, FObject vars, FObject pat, FObject tpl)
{
    FAssert(sizeof(FSyntaxRule) == sizeof(SyntaxRuleFieldsC) + sizeof(FRecord));

    FSyntaxRule * sr = (FSyntaxRule *) MakeRecord(R.SyntaxRuleRecordType);
    sr->NumVariables = MakeFixnum(nv);
    sr->Variables = vars;
    sr->Pattern = pat;
    sr->Template = tpl;

    return(sr);
}

#if 0
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
        for (int_t rc = 0; rc < AsFixnum(AsTemplateRepeat(ctpl)->RepeatCount); rc++)
            obj = MakePair(AsTemplateRepeat(ctpl)->Ellipsis, obj);

        return(MakePair(TemplateToList(AsTemplateRepeat(ctpl)->Template), obj));
    }

    return(ctpl);
}
#endif // 0

// ----------------

static FObject LiteralFind(FObject se, FObject list, FObject obj)
{
    FAssert(IdentifierP(obj));

    while (list != EmptyListObject)
    {
        FAssert(PairP(list));
        FAssert(ReferenceP(First(list)));

        if (MatchReference(First(list), se, obj))
            return(First(list));

        list = Rest(list);
    }

    return(NoValueObject);
}

static FObject CopyLiterals(FObject se, FObject obj, FObject ellip)
{
    FObject form = obj;
    FObject lits = EmptyListObject;

    while (obj != EmptyListObject)
    {
        if (PairP(obj) == 0 || (IdentifierP(First(obj)) == 0 && SymbolP(First(obj)) == 0))
            RaiseExceptionC(R.Syntax, "syntax-rules",
                    "expected list of identifiers for literals", List(obj, form));

        if (MatchReference(ellip, se, First(obj)))
            RaiseExceptionC(R.Syntax, "syntax-rules",
                    "<ellipsis> is not allowed as a literal", List(First(obj), form));

        if (ReferenceP(LiteralFind(se, lits, First(obj))))
            RaiseExceptionC(R.Syntax, "syntax-rules", "duplicate literal", List(First(obj), form));

        FObject id = First(obj);
        if (SymbolP(id))
            id = MakeIdentifier(id);

        lits = MakePair(MakeReference(ResolveIdentifier(se, id), id), lits);
        obj = Rest(obj);
    }

    return(lits);
}

static FObject PatternVariableFind(FObject se, FObject list, FObject var)
{
    FAssert(IdentifierP(var));

    while (list != EmptyListObject)
    {
        FAssert(PairP(list));
        FAssert(PatternVariableP(First(list)));

        if (MatchReference(AsPatternVariable(First(list))->Variable, se, var))
            return(First(list));

        list = Rest(list);
    }

    return(NoValueObject);
}

static FObject CompilePatternVariables(FObject se, FObject form, FObject lits, FObject pat,
    FObject ellip, FObject pvars, int_t rd)
{
    if (VectorP(pat))
        pat = VectorToList(pat);

    if (PairP(pat) && MatchReference(ellip, se, First(pat)))
        RaiseExceptionC(R.Syntax, "syntax-rules",
                "<ellipsis> must not start list pattern or vector pattern",
                List(pat, form));

    int_t ef = 0;
    while (PairP(pat))
    {
        if (PairP(Rest(pat)) && MatchReference(ellip, se, First(Rest(pat))))
        {
            if (ef)
                RaiseExceptionC(R.Syntax, "syntax-rules",
                        "more than one <ellipsis> in a list pattern or vector pattern",
                        List(pat, form));

            ef = 1;
            pvars = CompilePatternVariables(se, form, lits, First(pat), ellip, pvars,
                    rd + 1);
            pat = Rest(Rest(pat));
        }
        else
        {
            pvars = CompilePatternVariables(se, form, lits, First(pat), ellip, pvars, rd);
            pat = Rest(pat);
        }
    }

    if (SymbolP(pat))
        pat = MakeIdentifier(pat);

    if (IdentifierP(pat))
    {
        if (MatchReference(ellip, se, pat) == 0
                && MatchReference(R.UnderscoreReference, se, pat) == 0)
        {
            if (PatternVariableP(PatternVariableFind(se, pvars, pat)))
                RaiseExceptionC(R.Syntax, "syntax-rules",
                        "duplicate pattern variable", List(pat, form));

            if (ReferenceP(LiteralFind(se, lits, pat)) == 0)
                pvars = MakePair(MakePatternVariable(rd,
                                MakeReference(ResolveIdentifier(se, pat), pat)), pvars);
        }
    }

    return(pvars);
}

static void AssignVariableIndexes(FObject pvars, int_t idx)
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

static int_t CountPatternsAfterRepeat(FObject pat)
{
    int_t n = 0;

    while (PairP(pat))
    {
        n += 1;
        pat = Rest(pat);
    }

    return(n);
}

static FObject RepeatPatternVariables(FObject se, FObject pvars, FObject pat, FObject rvars)
{
    if (VectorP(pat))
        pat = VectorToList(pat);

    while (PairP(pat))
    {
        rvars = RepeatPatternVariables(se, pvars, First(pat), rvars);
        pat = Rest(pat);
    }

    if (SymbolP(pat))
        pat = MakeIdentifier(pat);

    if (IdentifierP(pat))
    {
        FObject var = PatternVariableFind(se, pvars, pat);
        if (PatternVariableP(var))
            return(MakePair(var, rvars));
    }

    return(rvars);
}

static FObject CompilePattern(FObject se, FObject lits, FObject pvars, FObject ellip, FObject pat)
{
    if (PairP(pat))
    {
        if (PairP(Rest(pat)) && MatchReference(ellip, se, First(Rest(pat))))
            return(MakePatternRepeat(CountPatternsAfterRepeat(Rest(Rest(pat))), ellip,
                    RepeatPatternVariables(se, pvars, First(pat), EmptyListObject),
                    CompilePattern(se, lits, pvars, ellip, First(pat)),
                    CompilePattern(se, lits, pvars, ellip, Rest(Rest(pat)))));

        return(MakePair(CompilePattern(se, lits, pvars, ellip, First(pat)),
                CompilePattern(se, lits, pvars, ellip, Rest(pat))));
    }
    else if (VectorP(pat))
        return(MakeVector(1, 0, CompilePattern(se, lits, pvars, ellip, VectorToList(pat))));
    else if (SymbolP(pat))
        pat = MakeIdentifier(pat);

    if (IdentifierP(pat))
    {
        FObject var = PatternVariableFind(se, pvars, pat);
        if (PatternVariableP(var))
            return(var);

        FObject ref = LiteralFind(se, lits, pat);
        if (ReferenceP(ref))
            return(ref);

        FAssert(MatchReference(R.UnderscoreReference, se, pat));

        return(MatchAnyObject);
    }

    return(pat);
}

static int_t ListFind(FObject list, FObject obj)
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

static int_t AddVarToTemplateRepeat(FObject var, FObject trs)
{
    int_t rd = AsFixnum(AsPatternVariable(var)->RepeatDepth);

    while (PairP(trs))
    {
        FObject tr = First(trs);
        FAssert(TemplateRepeatP(tr));

        for (int_t rc = 0; rc < AsFixnum(AsTemplateRepeat(tr)->RepeatCount); rc++)
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

static FObject CompileTemplate(FObject se, FObject form, FObject pvars, FObject ellip, FObject tpl,
    FObject trs, int_t qea)
{
    if (PairP(tpl))
    {
        if (qea != 0 && MatchReference(ellip, se, First(tpl)))
        {
            if (PairP(Rest(tpl)) == 0 || Rest(Rest(tpl)) != EmptyListObject)
                RaiseExceptionC(R.Syntax, "syntax-rules",
                        "expected (<ellipsis> <template>) or (... <template> <ellipsis> ...)",
                        List(tpl, form));

            return(CompileTemplate(se, form, pvars, NoValueObject, First(Rest(tpl)), trs,
                    0));
        }

        if (PairP(Rest(tpl)) && MatchReference(ellip, se, First(Rest(tpl))))
        {
            FObject rpt = First(tpl);
            int_t rc = 0;

            tpl = Rest(tpl);
            while (PairP(tpl) && MatchReference(ellip, se, First(tpl)))
            {
                rc += 1;
                tpl = Rest(tpl);
            }

            FAssert(rc > 0);

            FObject tr = MakeTemplateRepeat(ellip, rc);
//            AsTemplateRepeat(tr)->Template = CompileTemplate(se, form, pvars, ellip, rpt,
//                    MakePair(tr, trs), 1);
            Modify(FTemplateRepeat, tr, Template, CompileTemplate(se, form, pvars, ellip, rpt,
                    MakePair(tr, trs), 1));

            for (int_t rc = 0; rc < AsFixnum(AsTemplateRepeat(tr)->RepeatCount); rc++)
                if (AsVector(AsTemplateRepeat(tr)->Variables)->Vector[rc]
                        == EmptyListObject)
                    RaiseExceptionC(R.Syntax, "syntax-rules",
                            "missing repeated pattern variable for <ellipsis> in template",
                            List(tpl, form));

//            AsTemplateRepeat(tr)->Rest = CompileTemplate(se, form, pvars, ellip, tpl, trs,
//                    0);
            Modify(FTemplateRepeat, tr, Rest, CompileTemplate(se, form, pvars, ellip, tpl, trs,
                    0));

            return(tr);
        }

        return(MakePair(CompileTemplate(se, form, pvars, ellip, First(tpl), trs, 1),
                CompileTemplate(se, form, pvars, ellip, Rest(tpl), trs, 0)));
    }
    else if (VectorP(tpl))
        return(MakeVector(1, 0, CompileTemplate(se, form, pvars, ellip, VectorToList(tpl),
                trs, 0)));
    else if (SymbolP(tpl))
        tpl = MakeIdentifier(tpl);

    if (IdentifierP(tpl))
    {
        FObject var = PatternVariableFind(se, pvars, tpl);
        if (PatternVariableP(var))
        {
            if (AsFixnum(AsPatternVariable(var)->RepeatDepth) > 0)
            {
                if (trs == EmptyListObject || AddVarToTemplateRepeat(var, trs) == 0)
                    RaiseExceptionC(R.Syntax, "syntax-rules",
                            "missing <ellipsis> needed to repeat pattern variable in template",
                            List(tpl, form));
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
        RaiseExceptionC(R.Syntax, "syntax-rules",
                "expected (<pattern> <template>) for syntax rule", List(rule, form));

    FObject pat = First(rule);
    if (PairP(pat) == 0 || (IdentifierP(First(pat)) == 0 && SymbolP(First(pat)) == 0))
        RaiseExceptionC(R.Syntax, "syntax-rules",
                "pattern must be list starting with a symbol", List(pat, rule));

    FObject pvars = ReverseListModify(CompilePatternVariables(se, pat, lits, Rest(pat), ellip,
            EmptyListObject, 0));
    AssignVariableIndexes(pvars, 0);

    if (ListLength(pvars) > MaxPatternVars)
        RaiseExceptionC(R.Restriction, "syntax-rules", "too many pattern variables", List(rule));

    FObject cpat = CompilePattern(se, lits, pvars, ellip, Rest(pat));
    FObject tpl = CompileTemplate(se, First(Rest(rule)), pvars, ellip, First(Rest(rule)),
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
        ellip = MakeReference(ResolveIdentifier(se, First(Rest(obj))), First(Rest(obj)));
        obj = Rest(obj);
    }
    else if (PairP(Rest(obj)) && SymbolP(First(Rest(obj))))
    {
        FObject id = MakeIdentifier(First(Rest(obj)));
        ellip = MakeReference(ResolveIdentifier(se, id), id);
        obj = Rest(obj);
    }
    else
        ellip = R.EllipsisReference;

    if (PairP(Rest(obj)) == 0)
        RaiseExceptionC(R.Syntax, "syntax-rules", "missing literals", List(form));

    // (<literal> ...)

    FObject lits = ReverseListModify(CopyLiterals(se, First(Rest(obj)), ellip));

    FObject rules = Rest(Rest(obj));
    FObject nr = EmptyListObject;
    while (rules != EmptyListObject)
    {
        if (PairP(rules) == 0)
            RaiseExceptionC(R.Syntax, "syntax-rules", "expected a list of rules", List(form));

        if (PairP(First(rules)) == 0 || PairP(Rest(First(rules))) == 0
                || Rest(Rest(First(rules))) != EmptyListObject)
            RaiseExceptionC(R.Syntax, "syntax-rules", "expected (<pattern> <template>) for a rule",
                    List(First(rules), form));

        nr = MakePair(CompileRule(se, form, lits, First(rules), ellip), nr);
        rules = Rest(rules);
    }

    return(MakeSyntaxRules(lits, ReverseListModify(nr), se));
}

// ----------------

static void InitRepeatVariables(FObject vars, FObject vals[], FObject rvals[])
{
    while (PairP(vars))
    {
        FAssert(PatternVariableP(First(vars)));

        int_t idx = AsFixnum(AsPatternVariable(First(vars))->Index);
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

        int_t idx = AsFixnum(AsPatternVariable(First(vars))->Index);
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

static int_t MatchPattern(FObject se, FObject cpat, FObject vals[], FObject expr)
{
    for (;;)
    {
        FAssert(IdentifierP(cpat) == 0);
        FAssert(SymbolP(cpat) == 0);

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
        else if (MatchAnyObjectP(cpat))
            return(1);
        else if (ReferenceP(cpat))
            return(MatchReference(cpat, se, expr));
        else if (PatternVariableP(cpat))
        {
            vals[AsFixnum(AsPatternVariable(cpat)->Index)] = expr;
            return(1);
        }
        else if (PatternRepeatP(cpat))
        {
            int_t lc = AsFixnum(AsPatternRepeat(cpat)->LeaveCount);
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

static int_t CheckRepeatVariables(FObject vars, FObject vals[], FObject expr)
{
    while (PairP(vars))
    {
        FAssert(PatternVariableP(First(vars)));

        int_t idx = AsFixnum(AsPatternVariable(First(vars))->Index);
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

        int_t idx = AsFixnum(AsPatternVariable(First(vars))->Index);
        FAssert(idx >= 0 && idx < MaxPatternVars);

        FAssert(PairP(vals[idx]));
        rvals[idx] = First(vals[idx]);
        vals[idx] = Rest(vals[idx]);

        vars = Rest(vars);
    }

    FAssert(vars == EmptyListObject);
}

static FObject ExpandTemplate(FObject tse, FObject use, FObject ctpl, int_t nv, FObject vals[],
    FObject expr);
static FObject ExpandTemplateRepeat(FObject tse, FObject use, FObject ctpl, int_t nv, FObject vals[],
    int_t rc, FObject ret, FObject expr)
{
    FAssert(TemplateRepeatP(ctpl));
    FAssert(rc > 0);

    FObject rvals[MaxPatternVars];
    for (int_t vdx = 0; vdx < nv; vdx++)
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

        int_t idx = AsFixnum(AsPatternVariable(First(vars))->Index);
        FAssert(idx >= 0 && idx < MaxPatternVars);

        if (vals[idx] != EmptyListObject)
            RaiseExceptionC(R.Syntax, "syntax-rules",
                    "<ellipsis> match counts differ in template expansion", List(expr));

        vars = Rest(vars);
    }
    FAssert(vars == EmptyListObject);

    return(ret);
}

#if 0
static FObject CopyWrapValue(FObject se, FObject val)
{
    FAssert(SymbolP(val) == 0);

    if (IdentifierP(val))
        return(WrapIdentifier(val, se));

    if (PairP(val))
        return(MakePair(CopyWrapValue(se, First(val)), CopyWrapValue(se, Rest(val))));

    return(val);
}
#endif // 0

static FObject ExpandTemplate(FObject tse, FObject use, FObject ctpl, int_t nv, FObject vals[],
    FObject expr)
{
    if (PairP(ctpl))
        return(MakePair(ExpandTemplate(tse, use, First(ctpl), nv, vals, expr),
                ExpandTemplate(tse, use, Rest(ctpl), nv, vals, expr)));

    if (VectorP(ctpl))
        return(ListToVector(ExpandTemplate(tse, use, AsVector(ctpl)->Vector[0], nv, vals, expr)));

    if (PatternVariableP(ctpl))
        return(vals[AsFixnum(AsPatternVariable(ctpl)->Index)]);
//        return(CopyWrapValue(use, vals[AsFixnum(AsPatternVariable(ctpl)->Index)]));

    if (TemplateRepeatP(ctpl))
    {
        FObject rvals[MaxPatternVars];
        for (int_t vdx = 0; vdx < nv; vdx++)
            rvals[vdx] = vals[vdx];

        return(ExpandTemplateRepeat(tse, use, ctpl, nv, rvals,
                AsFixnum(AsTemplateRepeat(ctpl)->RepeatCount),
                ExpandTemplate(tse, use, AsTemplateRepeat(ctpl)->Rest, nv, vals, expr), expr));
    }

    FAssert(SymbolP(ctpl) == 0);

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
        for (int_t vdx = 0; vdx < AsFixnum(AsSyntaxRule(rule)->NumVariables); vdx++)
            vals[vdx] = NoValueObject;

        if (MatchPattern(se, AsSyntaxRule(rule)->Pattern, vals, expr))
            return(ExpandTemplate(MakeSyntacticEnv(AsSyntaxRules(sr)->SyntacticEnv), se,
                    AsSyntaxRule(rule)->Template, AsFixnum(AsSyntaxRule(rule)->NumVariables),
                    vals, expr));

        rules = Rest(rules);
    }

    RaiseExceptionC(R.Syntax, "syntax-rules", "unable to match pattern", List(sr, expr));
    return(NoValueObject);
}

void SetupSyntaxRules()
{
    R.SyntaxRulesRecordType = MakeRecordTypeC("syntax-rules",
            sizeof(SyntaxRulesFieldsC) / sizeof(char *), SyntaxRulesFieldsC);
    R.PatternVariableRecordType = MakeRecordTypeC("pattern-variable",
            sizeof(PatternVariableFieldsC) / sizeof(char *), PatternVariableFieldsC);
    R.PatternRepeatRecordType = MakeRecordTypeC("pattern-repeat",
            sizeof(PatternRepeatFieldsC) / sizeof(char *), PatternRepeatFieldsC);
    R.TemplateRepeatRecordType = MakeRecordTypeC("template-repeat",
            sizeof(TemplateRepeatFieldsC) / sizeof(char *), TemplateRepeatFieldsC);
    R.SyntaxRuleRecordType = MakeRecordTypeC("syntax-rule",
            sizeof(SyntaxRuleFieldsC) / sizeof(char *), SyntaxRuleFieldsC);
}
