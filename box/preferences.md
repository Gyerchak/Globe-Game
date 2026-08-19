# OpenCodeBox preferences — per-category ORDERED preferences.
#
# Semantics for every category: use the listed items IN ORDER. Only if one
# item fails may you fall back to the next one, and only after the whole list
# fails may you use other ways — and then say so.
# Environment variables and box buttons win over this file where they overlap.

[code]
prefer = c++20, c17, c++16
# Code conventions: how code should be written.
conventions = Modern, clean C++: RAII everywhere, const-correctness, standard
  library first, no raw new/delete, no macros where a constexpr/function
  works, small focused functions, explicit error handling (expected/optional
  or exceptions with clear messages), consistent naming (PascalCase types,
  camelCase functions, snake_case files), comments explain WHY not WHAT,
  format with clang-format style, compile with -Wall -Wextra clean.

[graphics]
prefer = vulkan

[build]
prefer = cmake, make
