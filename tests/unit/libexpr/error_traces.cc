#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "tests/libexpr.hh"

namespace nix {

    using namespace testing;

    // Testing eval of PrimOp's
    class ErrorTraceTest : public LibExprTest { };

    TEST_F(ErrorTraceTest, TraceBuilder) {
        ASSERT_THROW(
            state.error("Not much").debugThrow<EvalError>(),
            EvalError
        );

        ASSERT_THROW(
            state.error("Not much").withTrace(noPos, "No more").debugThrow<EvalError>(),
            EvalError
        );

        ASSERT_THROW(
            try {
                try {
                    state.error("Not much").withTrace(noPos, "No more").debugThrow<EvalError>();
                } catch (Error & e) {
                    e.addTrace(state.positions[noPos], "Something", "");
                    throw;
                }
            } catch (BaseError & e) {
                ASSERT_EQ(PrintToString(e.info().msg),
                          PrintToString(hintfmt("Not much")));
                auto trace = e.info().traces.rbegin();
                ASSERT_EQ(e.info().traces.size(), 2);
                ASSERT_EQ(PrintToString(trace->hint),
                          PrintToString(hintfmt("No more")));
                trace++;
                ASSERT_EQ(PrintToString(trace->hint),
                          PrintToString(hintfmt("Something")));
                throw;
            }
            , EvalError
        );
    }

    TEST_F(ErrorTraceTest, NestedThrows) {
        try {
            state.error("Not much").withTrace(noPos, "No more").debugThrow<EvalError>();
        } catch (BaseError & e) {
            try {
                state.error("Not much more").debugThrow<EvalError>();
            } catch (Error & e2) {
                e.addTrace(state.positions[noPos], "Something", "");
                //e2.addTrace(state.positions[noPos], "Something", "");
                ASSERT_TRUE(e.info().traces.size() == 2);
                ASSERT_TRUE(e2.info().traces.size() == 0);
                ASSERT_FALSE(&e.info() == &e2.info());
            }
        }
    }

#define ASSERT_TRACE1(args, type, message)                                  \
        ASSERT_THROW(                                                       \
            std::string expr(args);                                         \
            std::string name = expr.substr(0, expr.find(" "));              \
            try {                                                           \
                Value v = eval("builtins." args);                           \
                state.forceValueDeep(v);                                    \
            } catch (BaseError & e) {                                       \
                ASSERT_EQ(PrintToString(e.info().msg),                      \
                          PrintToString(message));                          \
                ASSERT_EQ(e.info().traces.size(), 1) << "while testing " args << std::endl << e.what(); \
                auto trace = e.info().traces.rbegin();                      \
                ASSERT_EQ(PrintToString(trace->hint),                       \
                          PrintToString(hintfmt("while calling the '%s' builtin", name))); \
                throw;                                                      \
            }                                                               \
            , type                                                          \
        )

#define ASSERT_TRACE2(args, type, message, context)                         \
        ASSERT_THROW(                                                       \
            std::string expr(args);                                         \
            std::string name = expr.substr(0, expr.find(" "));              \
            try {                                                           \
                Value v = eval("builtins." args);                           \
                state.forceValueDeep(v);                                    \
            } catch (BaseError & e) {                                       \
                ASSERT_EQ(PrintToString(e.info().msg),                      \
                          PrintToString(message));                          \
                ASSERT_EQ(e.info().traces.size(), 2) << "while testing " args << std::endl << e.what(); \
                auto trace = e.info().traces.rbegin();                      \
                ASSERT_EQ(PrintToString(trace->hint),                       \
                          PrintToString(context));                          \
                ++trace;                                                    \
                ASSERT_EQ(PrintToString(trace->hint),                       \
                          PrintToString(hintfmt("while calling the '%s' builtin", name))); \
                throw;                                                      \
            }                                                               \
            , type                                                          \
        )

    TEST_F(ErrorTraceTest, genericClosure) {
        ASSERT_TRACE2("genericClosure 1",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.genericClosure"));

        ASSERT_TRACE2("genericClosure {}",
                      TypeError,
                      hintfmt("attribute '%s' missing", "startSet"),
                      hintfmt("in the attrset passed as argument to builtins.genericClosure"));

        ASSERT_TRACE2("genericClosure { startSet = 1; }",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the 'startSet' attribute passed as argument to builtins.genericClosure"));

        ASSERT_TRACE2("genericClosure { startSet = [{ key = 1;}]; operator = true; }",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "a Boolean", ANSI_CYAN "true" ANSI_NORMAL),
                      hintfmt("while evaluating the 'operator' attribute passed as argument to builtins.genericClosure"));

        ASSERT_TRACE2("genericClosure { startSet = [{ key = 1;}]; operator = item: true; }",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a Boolean", ANSI_CYAN "true" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the `operator` passed to builtins.genericClosure"));

        ASSERT_TRACE2("genericClosure { startSet = [{ key = 1;}]; operator = item: [ true ]; }",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a Boolean", ANSI_CYAN "true" ANSI_NORMAL),
                      hintfmt("while evaluating one of the elements generated by (or initially passed to) builtins.genericClosure"));

        ASSERT_TRACE2("genericClosure { startSet = [{ key = 1;}]; operator = item: [ {} ]; }",
                      TypeError,
                      hintfmt("attribute '%s' missing", "key"),
                      hintfmt("in one of the attrsets generated by (or initially passed to) builtins.genericClosure"));

        ASSERT_TRACE2("genericClosure { startSet = [{ key = 1;}]; operator = item: [{ key = ''a''; }]; }",
                      EvalError,
                      hintfmt("cannot compare %s with %s", "a string", "an integer"),
                      hintfmt("while comparing the `key` attributes of two genericClosure elements"));

        ASSERT_TRACE2("genericClosure { startSet = [ true ]; operator = item: [{ key = ''a''; }]; }",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a Boolean", ANSI_CYAN "true" ANSI_NORMAL),
                      hintfmt("while evaluating one of the elements generated by (or initially passed to) builtins.genericClosure"));

    }


    TEST_F(ErrorTraceTest, replaceStrings) {
        ASSERT_TRACE2("replaceStrings 0 0 {}",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "0" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.replaceStrings"));

        ASSERT_TRACE2("replaceStrings [] 0 {}",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "0" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.replaceStrings"));

        ASSERT_TRACE1("replaceStrings [ 0 ] [] {}",
                      EvalError,
                      hintfmt("'from' and 'to' arguments passed to builtins.replaceStrings have different lengths"));

        ASSERT_TRACE2("replaceStrings [ 1 ] [ \"new\" ] {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating one of the strings to replace passed to builtins.replaceStrings"));

        ASSERT_TRACE2("replaceStrings [ \"oo\" ] [ true ] \"foo\"",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a Boolean", ANSI_CYAN "true" ANSI_NORMAL),
                      hintfmt("while evaluating one of the replacement strings passed to builtins.replaceStrings"));

        ASSERT_TRACE2("replaceStrings [ \"old\" ] [ \"new\" ] {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a set", "{ }"),
                      hintfmt("while evaluating the third argument passed to builtins.replaceStrings"));

    }


    TEST_F(ErrorTraceTest, scopedImport) {
    }


    TEST_F(ErrorTraceTest, import) {
    }


    TEST_F(ErrorTraceTest, typeOf) {
    }


    TEST_F(ErrorTraceTest, isNull) {
    }


    TEST_F(ErrorTraceTest, isFunction) {
    }


    TEST_F(ErrorTraceTest, isInt) {
    }


    TEST_F(ErrorTraceTest, isFloat) {
    }


    TEST_F(ErrorTraceTest, isString) {
    }


    TEST_F(ErrorTraceTest, isBool) {
    }


    TEST_F(ErrorTraceTest, isPath) {
    }


    TEST_F(ErrorTraceTest, break) {
    }


    TEST_F(ErrorTraceTest, abort) {
    }


    TEST_F(ErrorTraceTest, throw) {
    }


    TEST_F(ErrorTraceTest, addErrorContext) {
    }


    TEST_F(ErrorTraceTest, ceil) {
        ASSERT_TRACE2("ceil \"foo\"",
                      TypeError,
                      hintfmt("expected a float but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.ceil"));

    }


    TEST_F(ErrorTraceTest, floor) {
        ASSERT_TRACE2("floor \"foo\"",
                      TypeError,
                      hintfmt("expected a float but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.floor"));

    }


    TEST_F(ErrorTraceTest, tryEval) {
    }


    TEST_F(ErrorTraceTest, getEnv) {
        ASSERT_TRACE2("getEnv [ ]",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the first argument passed to builtins.getEnv"));

    }


    TEST_F(ErrorTraceTest, seq) {
    }


    TEST_F(ErrorTraceTest, deepSeq) {
    }


    TEST_F(ErrorTraceTest, trace) {
    }


    TEST_F(ErrorTraceTest, placeholder) {
        ASSERT_TRACE2("placeholder []",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the first argument passed to builtins.placeholder"));

    }


    TEST_F(ErrorTraceTest, toPath) {
        ASSERT_TRACE2("toPath []",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a list"),
                      hintfmt("while evaluating the first argument passed to builtins.toPath"));

        ASSERT_TRACE2("toPath \"foo\"",
                      EvalError,
                      hintfmt("string '%s' doesn't represent an absolute path", "foo"),
                      hintfmt("while evaluating the first argument passed to builtins.toPath"));

    }


    TEST_F(ErrorTraceTest, storePath) {
        ASSERT_TRACE2("storePath true",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a Boolean"),
                      hintfmt("while evaluating the first argument passed to 'builtins.storePath'"));

    }


    TEST_F(ErrorTraceTest, pathExists) {
        ASSERT_TRACE2("pathExists []",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a list"),
                      hintfmt("while realising the context of a path"));

        ASSERT_TRACE2("pathExists \"zorglub\"",
                      EvalError,
                      hintfmt("string '%s' doesn't represent an absolute path", "zorglub"),
                      hintfmt("while realising the context of a path"));

    }


    TEST_F(ErrorTraceTest, baseNameOf) {
        ASSERT_TRACE2("baseNameOf []",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a list"),
                      hintfmt("while evaluating the first argument passed to builtins.baseNameOf"));

    }


    TEST_F(ErrorTraceTest, dirOf) {
    }


    TEST_F(ErrorTraceTest, readFile) {
    }


    TEST_F(ErrorTraceTest, findFile) {
    }


    TEST_F(ErrorTraceTest, hashFile) {
    }


    TEST_F(ErrorTraceTest, readDir) {
    }


    TEST_F(ErrorTraceTest, toXML) {
    }


    TEST_F(ErrorTraceTest, toJSON) {
    }


    TEST_F(ErrorTraceTest, fromJSON) {
    }


    TEST_F(ErrorTraceTest, toFile) {
    }


    TEST_F(ErrorTraceTest, filterSource) {
        ASSERT_TRACE2("filterSource [] []",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a list"),
                      hintfmt("while evaluating the second argument (the path to filter) passed to 'builtins.filterSource'"));

        ASSERT_TRACE2("filterSource [] \"foo\"",
                      EvalError,
                      hintfmt("string '%s' doesn't represent an absolute path", "foo"),
                      hintfmt("while evaluating the second argument (the path to filter) passed to 'builtins.filterSource'"));

        ASSERT_TRACE2("filterSource [] ./.",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the first argument passed to builtins.filterSource"));

        // Usupported by store "dummy"

        // ASSERT_TRACE2("filterSource (_: 1) ./.",
        //               TypeError,
        //               hintfmt("attempt to call something which is not a function but %s", "an integer"),
        //               hintfmt("while adding path '/home/layus/projects/nix'"));

        // ASSERT_TRACE2("filterSource (_: _: 1) ./.",
        //               TypeError,
        //               hintfmt("expected a Boolean but found %s: %s", "an integer", "1"),
        //               hintfmt("while evaluating the return value of the path filter function"));

    }


    TEST_F(ErrorTraceTest, path) {
    }


    TEST_F(ErrorTraceTest, attrNames) {
        ASSERT_TRACE2("attrNames []",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the argument passed to builtins.attrNames"));

    }


    TEST_F(ErrorTraceTest, attrValues) {
        ASSERT_TRACE2("attrValues []",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the argument passed to builtins.attrValues"));

    }


    TEST_F(ErrorTraceTest, getAttr) {
        ASSERT_TRACE2("getAttr [] []",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the first argument passed to builtins.getAttr"));

        ASSERT_TRACE2("getAttr \"foo\" []",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the second argument passed to builtins.getAttr"));

        ASSERT_TRACE2("getAttr \"foo\" {}",
                      TypeError,
                      hintfmt("attribute '%s' missing", "foo"),
                      hintfmt("in the attribute set under consideration"));

    }


    TEST_F(ErrorTraceTest, unsafeGetAttrPos) {
    }


    TEST_F(ErrorTraceTest, hasAttr) {
        ASSERT_TRACE2("hasAttr [] []",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the first argument passed to builtins.hasAttr"));

        ASSERT_TRACE2("hasAttr \"foo\" []",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the second argument passed to builtins.hasAttr"));

    }


    TEST_F(ErrorTraceTest, isAttrs) {
    }


    TEST_F(ErrorTraceTest, removeAttrs) {
        ASSERT_TRACE2("removeAttrs \"\" \"\"",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a string", ANSI_MAGENTA "\"\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.removeAttrs"));

        ASSERT_TRACE2("removeAttrs \"\" [ 1 ]",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a string", ANSI_MAGENTA "\"\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.removeAttrs"));

        ASSERT_TRACE2("removeAttrs \"\" [ \"1\" ]",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a string", ANSI_MAGENTA "\"\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.removeAttrs"));

    }


    TEST_F(ErrorTraceTest, listToAttrs) {
        ASSERT_TRACE2("listToAttrs 1",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the argument passed to builtins.listToAttrs"));

        ASSERT_TRACE2("listToAttrs [ 1 ]",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating an element of the list passed to builtins.listToAttrs"));

        ASSERT_TRACE2("listToAttrs [ {} ]",
                      TypeError,
                      hintfmt("attribute '%s' missing", "name"),
                      hintfmt("in a {name=...; value=...;} pair"));

        ASSERT_TRACE2("listToAttrs [ { name = 1; } ]",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the `name` attribute of an element of the list passed to builtins.listToAttrs"));

        ASSERT_TRACE2("listToAttrs [ { name = \"foo\"; } ]",
                      TypeError,
                      hintfmt("attribute '%s' missing", "value"),
                      hintfmt("in a {name=...; value=...;} pair"));

    }


    TEST_F(ErrorTraceTest, intersectAttrs) {
        ASSERT_TRACE2("intersectAttrs [] []",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the first argument passed to builtins.intersectAttrs"));

        ASSERT_TRACE2("intersectAttrs {} []",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the second argument passed to builtins.intersectAttrs"));

    }


    TEST_F(ErrorTraceTest, catAttrs) {
        ASSERT_TRACE2("catAttrs [] {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the first argument passed to builtins.catAttrs"));

        ASSERT_TRACE2("catAttrs \"foo\" {}",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a set", "{ }"),
                      hintfmt("while evaluating the second argument passed to builtins.catAttrs"));

        ASSERT_TRACE2("catAttrs \"foo\" [ 1 ]",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating an element in the list passed as second argument to builtins.catAttrs"));

        ASSERT_TRACE2("catAttrs \"foo\" [ { foo = 1; } 1 { bar = 5;} ]",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating an element in the list passed as second argument to builtins.catAttrs"));

    }


    TEST_F(ErrorTraceTest, functionArgs) {
        ASSERT_TRACE1("functionArgs {}",
                      TypeError,
                      hintfmt("'functionArgs' requires a function"));

    }


    TEST_F(ErrorTraceTest, mapAttrs) {
        ASSERT_TRACE2("mapAttrs [] []",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the second argument passed to builtins.mapAttrs"));

        // XXX: defered
        // ASSERT_TRACE2("mapAttrs \"\" { foo.bar = 1; }",
        //               TypeError,
        //               hintfmt("attempt to call something which is not a function but %s", "a string"),
        //               hintfmt("while evaluating the attribute 'foo'"));

        // ASSERT_TRACE2("mapAttrs (x: x + \"1\") { foo.bar = 1; }",
        //               TypeError,
        //               hintfmt("attempt to call something which is not a function but %s", "a string"),
        //               hintfmt("while evaluating the attribute 'foo'"));

        // ASSERT_TRACE2("mapAttrs (x: y: x + 1) { foo.bar = 1; }",
        //               TypeError,
        //               hintfmt("cannot coerce %s to a string", "an integer"),
        //               hintfmt("while evaluating a path segment"));

    }


    TEST_F(ErrorTraceTest, zipAttrsWith) {
        ASSERT_TRACE2("zipAttrsWith [] [ 1 ]",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "a list", "[ ]"),
                      hintfmt("while evaluating the first argument passed to builtins.zipAttrsWith"));

        ASSERT_TRACE2("zipAttrsWith (_: 1) [ 1 ]",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating a value of the list passed as second argument to builtins.zipAttrsWith"));

        // XXX: How to properly tell that the fucntion takes two arguments ?
        // The same question also applies to sort, and maybe others.
        // Due to lazyness, we only create a thunk, and it fails later on.
        // ASSERT_TRACE2("zipAttrsWith (_: 1) [ { foo = 1; } ]",
        //               TypeError,
        //               hintfmt("attempt to call something which is not a function but %s", "an integer"),
        //               hintfmt("while evaluating the attribute 'foo'"));

        // XXX: Also deferred deeply
        // ASSERT_TRACE2("zipAttrsWith (a: b: a + b) [ { foo = 1; } { foo = 2; } ]",
        //               TypeError,
        //               hintfmt("cannot coerce %s to a string", "a list"),
        //               hintfmt("while evaluating a path segment"));

    }


    TEST_F(ErrorTraceTest, isList) {
    }


    TEST_F(ErrorTraceTest, elemAt) {
        ASSERT_TRACE2("elemAt \"foo\" (-1)",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.elemAt"));

        ASSERT_TRACE1("elemAt [] (-1)",
                      Error,
                      hintfmt("list index %d is out of bounds", -1));

        ASSERT_TRACE1("elemAt [\"foo\"] 3",
                      Error,
                      hintfmt("list index %d is out of bounds", 3));

    }


    TEST_F(ErrorTraceTest, head) {
        ASSERT_TRACE2("head 1",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.elemAt"));

        ASSERT_TRACE1("head []",
                      Error,
                      hintfmt("list index %d is out of bounds", 0));

    }


    TEST_F(ErrorTraceTest, tail) {
        ASSERT_TRACE2("tail 1",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.tail"));

        ASSERT_TRACE1("tail []",
                      Error,
                      hintfmt("'tail' called on an empty list"));

    }


    TEST_F(ErrorTraceTest, map) {
        ASSERT_TRACE2("map 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.map"));

        ASSERT_TRACE2("map 1 [ 1 ]",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.map"));

    }


    TEST_F(ErrorTraceTest, filter) {
        ASSERT_TRACE2("filter 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.filter"));

        ASSERT_TRACE2("filter 1 [ \"foo\" ]",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.filter"));

        ASSERT_TRACE2("filter (_: 5) [ \"foo\" ]",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "an integer", ANSI_CYAN "5" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the filtering function passed to builtins.filter"));

    }


    TEST_F(ErrorTraceTest, elem) {
        ASSERT_TRACE2("elem 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.elem"));

    }


    TEST_F(ErrorTraceTest, concatLists) {
        ASSERT_TRACE2("concatLists 1",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.concatLists"));

        ASSERT_TRACE2("concatLists [ 1 ]",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating a value of the list passed to builtins.concatLists"));

        ASSERT_TRACE2("concatLists [ [1] \"foo\" ]",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating a value of the list passed to builtins.concatLists"));

    }


    TEST_F(ErrorTraceTest, length) {
        ASSERT_TRACE2("length 1",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.length"));

        ASSERT_TRACE2("length \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.length"));

    }


    TEST_F(ErrorTraceTest, foldlPrime) {
        ASSERT_TRACE2("foldl' 1 \"foo\" true",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.foldlStrict"));

        ASSERT_TRACE2("foldl' (_: 1) \"foo\" true",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a Boolean", ANSI_CYAN "true" ANSI_NORMAL),
                      hintfmt("while evaluating the third argument passed to builtins.foldlStrict"));

        ASSERT_TRACE1("foldl' (_: 1) \"foo\" [ true ]",
                      TypeError,
                      hintfmt("attempt to call something which is not a function but %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL));

        ASSERT_TRACE2("foldl' (a: b: a && b) \"foo\" [ true ]",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("in the left operand of the AND (&&) operator"));

    }


    TEST_F(ErrorTraceTest, any) {
        ASSERT_TRACE2("any 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.any"));

        ASSERT_TRACE2("any (_: 1) \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.any"));

        ASSERT_TRACE2("any (_: 1) [ \"foo\" ]",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the function passed to builtins.any"));

    }


    TEST_F(ErrorTraceTest, all) {
        ASSERT_TRACE2("all 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.all"));

        ASSERT_TRACE2("all (_: 1) \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.all"));

        ASSERT_TRACE2("all (_: 1) [ \"foo\" ]",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the function passed to builtins.all"));

    }


    TEST_F(ErrorTraceTest, genList) {
        ASSERT_TRACE2("genList 1 \"foo\"",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.genList"));

        ASSERT_TRACE2("genList 1 2",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.genList", "an integer"));

        // XXX: defered
        // ASSERT_TRACE2("genList (x: x + \"foo\") 2 #TODO",
        //               TypeError,
        //               hintfmt("cannot add %s to an integer", "a string"),
        //               hintfmt("while evaluating anonymous lambda"));

        ASSERT_TRACE1("genList false (-3)",
                      EvalError,
                      hintfmt("cannot create list of size %d", -3));

    }


    TEST_F(ErrorTraceTest, sort) {
        ASSERT_TRACE2("sort 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.sort"));

        ASSERT_TRACE2("sort 1 [ \"foo\" ]",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.sort"));

        ASSERT_TRACE1("sort (_: 1) [ \"foo\" \"bar\" ]",
                      TypeError,
                      hintfmt("attempt to call something which is not a function but %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL));

        ASSERT_TRACE2("sort (_: _: 1) [ \"foo\" \"bar\" ]",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the sorting function passed to builtins.sort"));

        // XXX: Trace too deep, need better asserts
        // ASSERT_TRACE1("sort (a: b: a <= b) [ \"foo\" {} ] # TODO",
        //               TypeError,
        //               hintfmt("cannot compare %s with %s", "a string", "a set"));

        // ASSERT_TRACE1("sort (a: b: a <= b) [ {} {} ] # TODO",
        //               TypeError,
        //               hintfmt("cannot compare %s with %s; values of that type are incomparable", "a set", "a set"));

    }


    TEST_F(ErrorTraceTest, partition) {
        ASSERT_TRACE2("partition 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.partition"));

        ASSERT_TRACE2("partition (_: 1) \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.partition"));

        ASSERT_TRACE2("partition (_: 1) [ \"foo\" ]",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the partition function passed to builtins.partition"));

    }


    TEST_F(ErrorTraceTest, groupBy) {
        ASSERT_TRACE2("groupBy 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.groupBy"));

        ASSERT_TRACE2("groupBy (_: 1) \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.groupBy"));

        ASSERT_TRACE2("groupBy (x: x) [ \"foo\" \"bar\" 1 ]",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the grouping function passed to builtins.groupBy"));

    }


    TEST_F(ErrorTraceTest, concatMap) {
        ASSERT_TRACE2("concatMap 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a function but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.concatMap"));

        ASSERT_TRACE2("concatMap (x: 1) \"foo\"",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.concatMap"));

        ASSERT_TRACE2("concatMap (x: 1) [ \"foo\" ] # TODO",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the function passed to builtins.concatMap"));

        ASSERT_TRACE2("concatMap (x: \"foo\") [ 1 2 ] # TODO",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the return value of the function passed to builtins.concatMap"));

    }


    TEST_F(ErrorTraceTest, add) {
        ASSERT_TRACE2("add \"foo\" 1",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument of the addition"));

        ASSERT_TRACE2("add 1 \"foo\"",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument of the addition"));

    }


    TEST_F(ErrorTraceTest, sub) {
        ASSERT_TRACE2("sub \"foo\" 1",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument of the subtraction"));

        ASSERT_TRACE2("sub 1 \"foo\"",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument of the subtraction"));

    }


    TEST_F(ErrorTraceTest, mul) {
        ASSERT_TRACE2("mul \"foo\" 1",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument of the multiplication"));

        ASSERT_TRACE2("mul 1 \"foo\"",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument of the multiplication"));

    }


    TEST_F(ErrorTraceTest, div) {
        ASSERT_TRACE2("div \"foo\" 1 # TODO: an integer was expected -> a number",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the first operand of the division"));

        ASSERT_TRACE2("div 1 \"foo\"",
                      TypeError,
                      hintfmt("expected a float but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second operand of the division"));

        ASSERT_TRACE1("div \"foo\" 0",
                      EvalError,
                      hintfmt("division by zero"));

    }


    TEST_F(ErrorTraceTest, bitAnd) {
        ASSERT_TRACE2("bitAnd 1.1 2",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a float", ANSI_CYAN "1.1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.bitAnd"));

        ASSERT_TRACE2("bitAnd 1 2.2",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a float", ANSI_CYAN "2.2" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.bitAnd"));

    }


    TEST_F(ErrorTraceTest, bitOr) {
        ASSERT_TRACE2("bitOr 1.1 2",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a float", ANSI_CYAN "1.1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.bitOr"));

        ASSERT_TRACE2("bitOr 1 2.2",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a float", ANSI_CYAN "2.2" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.bitOr"));

    }


    TEST_F(ErrorTraceTest, bitXor) {
        ASSERT_TRACE2("bitXor 1.1 2",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a float", ANSI_CYAN "1.1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.bitXor"));

        ASSERT_TRACE2("bitXor 1 2.2",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a float", ANSI_CYAN "2.2" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument passed to builtins.bitXor"));

    }


    TEST_F(ErrorTraceTest, lessThan) {
        ASSERT_TRACE1("lessThan 1 \"foo\"",
                      EvalError,
                      hintfmt("cannot compare %s with %s", "an integer", "a string"));

        ASSERT_TRACE1("lessThan {} {}",
                      EvalError,
                      hintfmt("cannot compare %s with %s; values of that type are incomparable", "a set", "a set"));

        ASSERT_TRACE2("lessThan [ 1 2 ] [ \"foo\" ]",
                      EvalError,
                      hintfmt("cannot compare %s with %s", "an integer", "a string"),
                      hintfmt("while comparing two list elements"));

    }


    TEST_F(ErrorTraceTest, toString) {
        ASSERT_TRACE2("toString { a = 1; }",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a set"),
                      hintfmt("while evaluating the first argument passed to builtins.toString"));

    }


    TEST_F(ErrorTraceTest, substring) {
        ASSERT_TRACE2("substring {} \"foo\" true",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a set", "{ }"),
                      hintfmt("while evaluating the first argument (the start offset) passed to builtins.substring"));

        ASSERT_TRACE2("substring 3 \"foo\" true",
                      TypeError,
                      hintfmt("expected an integer but found %s: %s", "a string", ANSI_MAGENTA "\"foo\"" ANSI_NORMAL),
                      hintfmt("while evaluating the second argument (the substring length) passed to builtins.substring"));

        ASSERT_TRACE2("substring 0 3 {}",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a set"),
                      hintfmt("while evaluating the third argument (the string) passed to builtins.substring"));

        ASSERT_TRACE1("substring (-3) 3 \"sometext\"",
                      EvalError,
                      hintfmt("negative start position in 'substring'"));

    }


    TEST_F(ErrorTraceTest, stringLength) {
        ASSERT_TRACE2("stringLength {} # TODO: context is missing ???",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a set"),
                      hintfmt("while evaluating the argument passed to builtins.stringLength"));

    }


    TEST_F(ErrorTraceTest, hashString) {
        ASSERT_TRACE2("hashString 1 {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.hashString"));

        ASSERT_TRACE1("hashString \"foo\" \"content\"",
                      UsageError,
                      hintfmt("unknown hash algorithm '%s', expect 'md5', 'sha1', 'sha256', or 'sha512'", "foo"));

        ASSERT_TRACE2("hashString \"sha256\" {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a set", "{ }"),
                      hintfmt("while evaluating the second argument passed to builtins.hashString"));

    }


    TEST_F(ErrorTraceTest, match) {
        ASSERT_TRACE2("match 1 {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.match"));

        ASSERT_TRACE2("match \"foo\" {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a set", "{ }"),
                      hintfmt("while evaluating the second argument passed to builtins.match"));

        ASSERT_TRACE1("match \"(.*\" \"\"",
                      EvalError,
                      hintfmt("invalid regular expression '%s'", "(.*"));

    }


    TEST_F(ErrorTraceTest, split) {
        ASSERT_TRACE2("split 1 {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.split"));

        ASSERT_TRACE2("split \"foo\" {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a set", "{ }"),
                      hintfmt("while evaluating the second argument passed to builtins.split"));

        ASSERT_TRACE1("split \"f(o*o\" \"1foo2\"",
                      EvalError,
                      hintfmt("invalid regular expression '%s'", "f(o*o"));

    }


    TEST_F(ErrorTraceTest, concatStringsSep) {
        ASSERT_TRACE2("concatStringsSep 1 {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument (the separator string) passed to builtins.concatStringsSep"));

        ASSERT_TRACE2("concatStringsSep \"foo\" {}",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a set", "{ }"),
                      hintfmt("while evaluating the second argument (the list of strings to concat) passed to builtins.concatStringsSep"));

        ASSERT_TRACE2("concatStringsSep \"foo\" [ 1 2 {} ] # TODO: coerce to string is buggy",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "an integer"),
                      hintfmt("while evaluating one element of the list of strings to concat passed to builtins.concatStringsSep"));

    }


    TEST_F(ErrorTraceTest, parseDrvName) {
        ASSERT_TRACE2("parseDrvName 1",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.parseDrvName"));

    }


    TEST_F(ErrorTraceTest, compareVersions) {
        ASSERT_TRACE2("compareVersions 1 {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.compareVersions"));

        ASSERT_TRACE2("compareVersions \"abd\" {}",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "a set", "{ }"),
                      hintfmt("while evaluating the second argument passed to builtins.compareVersions"));

    }


    TEST_F(ErrorTraceTest, splitVersion) {
        ASSERT_TRACE2("splitVersion 1",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", ANSI_CYAN "1" ANSI_NORMAL),
                      hintfmt("while evaluating the first argument passed to builtins.splitVersion"));

    }


    TEST_F(ErrorTraceTest, traceVerbose) {
    }


    /* // Needs different ASSERTs
    TEST_F(ErrorTraceTest, derivationStrict) {
        ASSERT_TRACE2("derivationStrict \"\"",
                      TypeError,
                      hintfmt("expected a set but found %s: %s", "a string", "\"\""),
                      hintfmt("while evaluating the argument passed to builtins.derivationStrict"));

        ASSERT_TRACE2("derivationStrict {}",
                      TypeError,
                      hintfmt("attribute '%s' missing", "name"),
                      hintfmt("in the attrset passed as argument to builtins.derivationStrict"));

        ASSERT_TRACE2("derivationStrict { name = 1; }",
                      TypeError,
                      hintfmt("expected a string but found %s: %s", "an integer", "1"),
                      hintfmt("while evaluating the `name` attribute passed to builtins.derivationStrict"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; }",
                      TypeError,
                      hintfmt("required attribute 'builder' missing"),
                      hintfmt("while evaluating derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; __structuredAttrs = 15; }",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "an integer", "15"),
                      hintfmt("while evaluating the `__structuredAttrs` attribute passed to builtins.derivationStrict"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; __ignoreNulls = 15; }",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "an integer", "15"),
                      hintfmt("while evaluating the `__ignoreNulls` attribute passed to builtins.derivationStrict"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; outputHashMode = 15; }",
                      TypeError,
                      hintfmt("invalid value '15' for 'outputHashMode' attribute"),
                      hintfmt("while evaluating the attribute 'outputHashMode' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; outputHashMode = \"custom\"; }",
                      TypeError,
                      hintfmt("invalid value 'custom' for 'outputHashMode' attribute"),
                      hintfmt("while evaluating the attribute 'outputHashMode' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = {}; }",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a set"),
                      hintfmt("while evaluating the attribute 'system' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = {}; }",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a set"),
                      hintfmt("while evaluating the attribute 'outputs' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = \"drv\"; }",
                      TypeError,
                      hintfmt("invalid derivation output name 'drv'"),
                      hintfmt("while evaluating the attribute 'outputs' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = []; }",
                      TypeError,
                      hintfmt("derivation cannot have an empty set of outputs"),
                      hintfmt("while evaluating the attribute 'outputs' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = [ \"drv\" ]; }",
                      TypeError,
                      hintfmt("invalid derivation output name 'drv'"),
                      hintfmt("while evaluating the attribute 'outputs' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = [ \"out\" \"out\" ]; }",
                      TypeError,
                      hintfmt("duplicate derivation output 'out'"),
                      hintfmt("while evaluating the attribute 'outputs' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = \"out\"; __contentAddressed = \"true\"; }",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "a string", "\"true\""),
                      hintfmt("while evaluating the attribute '__contentAddressed' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = \"out\"; __impure = \"true\"; }",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "a string", "\"true\""),
                      hintfmt("while evaluating the attribute '__impure' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = \"out\"; __impure = \"true\"; }",
                      TypeError,
                      hintfmt("expected a Boolean but found %s: %s", "a string", "\"true\""),
                      hintfmt("while evaluating the attribute '__impure' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = \"out\"; args = \"foo\"; }",
                      TypeError,
                      hintfmt("expected a list but found %s: %s", "a string", "\"foo\""),
                      hintfmt("while evaluating the attribute 'args' of derivation 'foo'"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = \"out\"; args = [ {} ]; }",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a set"),
                      hintfmt("while evaluating an element of the argument list"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = \"out\"; args = [ \"a\" {} ]; }",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a set"),
                      hintfmt("while evaluating an element of the argument list"));

        ASSERT_TRACE2("derivationStrict { name = \"foo\"; builder = 1; system = 1; outputs = \"out\"; FOO = {}; }",
                      TypeError,
                      hintfmt("cannot coerce %s to a string", "a set"),
                      hintfmt("while evaluating the attribute 'FOO' of derivation 'foo'"));

    }
    */

} /* namespace nix */
