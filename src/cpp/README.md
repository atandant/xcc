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
| token pasting with `##` | implemented |
| `#undef` and null directives | implemented |
| function-like macros and stringification with `#` | implemented |
| `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif` | implemented |
| `#include` and `-I` include search paths | experimental |
| `#error` | implemented |
| `#line` and `#pragma` | not implemented |
| `__LINE__`, `__FILE__`, `__DATE__`, `__TIME__`, `__STDC__`, `__XCC__` | implemented |
| `-D` and `-U` command-line macro actions | implemented |
| `-E` preprocessing-only output | not implemented |

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

Macro definitions and expansion are owned by a unified engine in
`src/cpp/macro.c`. Replacement lists are stored as tokens and rescanned at use
time. Every expanded token carries an immutable hide set, which terminates
direct and mutual recursion without using an arbitrary depth limit.
Function-like arguments retain separate raw and lazily prescanned forms:
ordinary parameters use the prescanned form, while `#` and `##` use raw tokens.
Stringification normalizes whitespace and escapes literal spellings. `##`
uses placemarkers for empty arguments, requires each nonempty concatenation to
form exactly one preprocessing token, and rescans the result.

The C89 predefined macros and xcc's `__XCC__` identification macro live in the
same macro table as source and command-line definitions. `__LINE__` and
`__FILE__` are expanded from each invocation location; `__DATE__` and
`__TIME__` are fixed when preprocessing begins and honor `SOURCE_DATE_EPOCH`
for reproducible builds. Builtins are installed before ordered `-D`/`-U`
actions, so `-U` can suppress one before source processing.

An active `#error` always emits a diagnostic containing its unexpanded
preprocessing-token sequence. Like other non-conditional directives, an
`#error` in an inactive conditional group is discarded.

Conditional groups use an explicit nesting stack. Tokens in inactive groups
are discarded before macro expansion, while nested conditional directives are
still recognized. Active `#if` and `#elif` lines resolve `defined`, undergo
bounded macro expansion, replace remaining identifiers with zero, and are
evaluated by a preprocessor-only C89 integer expression parser. The evaluator
does not depend on the C parser, AST, type system, or semantic state.

Included files are processed by a stack of retained input frames. Each file
independently undergoes trigraph replacement, line splicing, comment handling,
and preprocessing-token scanning while sharing the translation unit's macro
table. Quoted includes search the including file's directory before repeated
`-I` directories; angle includes search only `-I` directories. Conditional
groups cannot cross file boundaries, and nesting is limited to 200 files.

All input is retained for the compilation lifetime. Token locations identify
their source file, including nested headers, without a mutable global filename.
Expanded object-like tokens currently use the invocation location; nested
macro-expansion notes are intentionally deferred.

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

The corpus currently includes 50 object-macro-focused cases, 47 function-macro
cases, 28 conditional-inclusion cases, and 25 include cases in addition to the
phase-scanner cases. It covers argument collection and lazy prescan,
rescanning, direct and mutual recursion, hide-set boundaries, stringification,
placemarkers, token-paste re-lexing, definition order, redefinition, `#undef`,
trigraph and splice interactions, inactive groups, conditional nesting and
diagnostics, C89 expression evaluation, nested and macro-expanded includes,
search order, include guards, recursion limits, and malformed input.
Source-level tests should continue to cover phase interactions and edge
conditions, not only happy paths.

## Next milestone

`#include` remains experimental while its header-name edge cases, file-system
failures, and real-world nested-header coverage mature. Variadic macros remain
out of scope because they are not C89.

## Explicit non-goals for now

- multiple translation units, assembling, or linking
- compatibility with arbitrary host system headers
- GNU variadic macros and other compiler-specific extensions
- dependency generation, precompiled headers, or include caching
- Clang-style source ranges and macro diagnostic traces
