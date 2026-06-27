# Third-party tools

xcc uses the following programs **as build-time tools only**. They are not
linked into xcc, and xcc does not redistribute their source.

- **GNU Bison** — generates the parser from `src/parser.y`.
  Bison is GPLv3, but its generated parser output carries the *Bison Parser
  Exception*, which permits using and distributing the generated code under any
  license. This is why xcc can be MIT-licensed.

- **flex** — generates the lexical scanner from `src/lexer.l`.
  flex is distributed under a permissive BSD-style license; its generated
  scanner imposes no copyleft on xcc.

The generated files are produced into `build/` at compile time and are **not**
committed to this repository, so no third-party headers are redistributed here.
If you ever vendor the generated `parser.c`/`lexer.c`, keep their copyright and
license headers intact.
