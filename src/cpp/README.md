# xcc native preprocessor

> **Work in progress:** this subsystem is the production input path for xcc,
> but much of C89 preprocessing is not implemented yet. Unsupported directives
> and macro forms are rejected rather than ignored or miscompiled.

## Status

| Area | Status |
| --- | --- |
| retained source input and file-aware locations | implemented |
| C89 trigraph replacement | implemented |
| backslash-newline splicing | implemented |
| preprocessing-token scanning | implemented |
| block comments replaced by whitespace | implemented |
| `//` comments | implemented xcc extension |
| parser-token conversion | implemented |
| object-like `#define` | implemented |
| replacement rescanning and token hide sets | implemented |
| object-like token pasting with `##` | experimental |
| `#undef` and null directives | implemented |
| function-like macros and stringification with `#` | not implemented |
| `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif` | implemented |
| `#include` and include search paths | not implemented |
| `#line`, `#error`, and `#pragma` | not implemented |
| predefined macros | not implemented |
| `-D`, `-U`, `-I`, and `-E` | not implemented |

“Implemented” is a supported contract with broad regression coverage.
“Experimental” means the documented subset is intended to work and is tested,
but has not yet accumulated enough conformance coverage to be considered
stable. “Partial” is reserved for features with known valid cases that still
fail explicitly. “Not implemented” means no support should be assumed.

## Architecture

The parser never consumes regenerated preprocessing text:

```text
retained source
    -> phase 1-3 raw scanner
    -> directives and macro expansion (`cpp_next`)
    -> expanded preprocessing tokens
    -> C token adapter (`yylex`)
    -> Bison parser
```

`cpp_next` preserves preprocessing-token spelling and reports whether a token
starts a logical line or has leading whitespace. The adapter in `src/lexer.c`
performs keyword recognition, literal decoding, integer conversion, and
parser-dependent typedef-name classification. `src/cpp` must not depend on
the parser, AST, or semantic state.

Object-like replacement lists are stored as tokens and rescanned at use time.
Every expanded token carries an immutable hide set, which terminates direct and
mutual recursion without using an arbitrary depth limit. `##` concatenates
spellings, requires the result to be exactly one preprocessing token, and then
rescans it. Function-like argument substitution will build on this mechanism,
but needs additional hide-set composition rules and must not blindly reuse the
object-like substitution helper.

Conditional groups use an explicit nesting stack. Tokens in inactive groups
are discarded before macro expansion, while nested conditional directives are
still recognized. Active `#if` and `#elif` lines resolve `defined`, undergo
bounded macro expansion, replace remaining identifiers with zero, and are
evaluated by a preprocessor-only C89 integer expression parser. The evaluator
does not depend on the C parser, AST, type system, or semantic state.

All input is retained for the compilation lifetime. Token locations identify
their source file so future included files do not require a mutable global
filename. Expanded object-like tokens currently use the invocation location;
nested macro-expansion notes are intentionally deferred.

## Translation-phase behavior

Trigraphs are replaced before backslash-newline splicing. Splicing is complete
before comments are recognized, including for the supported `//` extension.
Comments act as whitespace and cannot accidentally join adjacent tokens.

The scanner normalizes CRLF and lone CR input newlines. Diagnostic locations
continue to refer to physical source lines and columns.

## Tests

Preprocessor acceptance cases live in `tests/cpp/` and use the same
`/* expect: N */` and `/* expect-error: text */` headers as `tests/cases/`.
They are run by `tests/run-cpp.sh` as part of:

```sh
make test
```

The corpus currently includes 50 object-macro-focused cases and 28 conditional
inclusion cases in addition to the phase-scanner cases. It covers rescanning,
direct and mutual recursion, definition order, redefinition, `#undef`,
trigraph and splice interactions, token-paste re-lexing, inactive groups,
conditional nesting and diagnostics, C89 expression evaluation, and malformed
input. Source-level tests should continue to cover phase interactions and edge
conditions, not only happy paths.

## Next milestone

The next macro milestone is complete C89 function-like replacement: parameter
parsing, balanced argument collection, argument prescan, stringification,
parameter-aware token pasting (including empty arguments), and the additional
hide-set propagation required during argument substitution. Variadic macros
remain out of scope because they are not C89.

## Explicit non-goals for now

- multiple translation units, assembling, or linking
- compatibility with arbitrary host system headers
- GNU variadic macros and other compiler-specific extensions
- dependency generation, precompiled headers, or include caching
- Clang-style source ranges and macro diagnostic traces
