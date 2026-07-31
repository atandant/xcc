# xcc native preprocessor

> **Work in progress:** this subsystem is the production input path for xcc,
> but preprocessing directives, macro expansion, and includes are not
> implemented yet. Unsupported directives are rejected rather than ignored.

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
| `#define`, `#undef`, and macro expansion | not implemented |
| `#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif` | not implemented |
| `#include` and include search paths | not implemented |
| `#line`, `#error`, and `#pragma` | not implemented |
| predefined macros | not implemented |
| `-D`, `-U`, `-I`, and `-E` | not implemented |

“Implemented” means exercised through xcc's test suite. “Not implemented”
means no support should be assumed; directive input currently produces an
explicit diagnostic.

## Architecture

The parser never consumes regenerated preprocessing text:

```text
retained source
    -> phase 1-3 scanner (`cpp_next`)
    -> expanded preprocessing tokens (currently pass-through)
    -> C token adapter (`yylex`)
    -> Bison parser
```

`cpp_next` preserves preprocessing-token spelling and reports whether a token
starts a logical line or has leading whitespace. The adapter in `src/lexer.c`
performs keyword recognition, literal decoding, integer conversion, and
parser-dependent typedef-name classification. `src/cpp` must not depend on
the parser, AST, or semantic state.

All input is retained for the compilation lifetime. Token locations identify
their source file so future included files do not require a mutable global
filename. Full macro-expansion provenance is intentionally deferred until
macro expansion exists.

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

The corpus is expected to grow substantially as each directive and macro rule
is implemented. Source-level tests should cover phase interactions and edge
conditions, not only ordinary C that happens to pass through the scanner.

## Next milestone

The first directive milestone will be selected only after the pass-through
scanner remains stable. It should be a deliberately small vertical slice,
most likely object-like `#define` plus `#undef`, with token-based replacement,
rescanning, and recursion suppression tested before function-like macros are
attempted.

## Explicit non-goals for now

- multiple translation units, assembling, or linking
- compatibility with arbitrary host system headers
- GNU variadic macros and other compiler-specific extensions
- dependency generation, precompiled headers, or include caching
- Clang-style source ranges and macro diagnostic traces
