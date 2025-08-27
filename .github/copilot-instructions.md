## RadiantEngine – Copilot Style & Standards (Single Source)

ONLY style / formatting / naming / density rules for AI-generated C++ in this repo. Architecture, build, runtime notes intentionally omitted here so the model focuses on code shape. Assume: C++20, Vulkan, Windows / Linux (MSVC/Clang), 100-col soft wrap.

### Control Flow & Braces
1. Always use braces for every `if/else/for/while/do` body (no brace elision) even for single statements.
2. Never write a control statement + its body on the same physical line: disallow `if (x) return;`.
3. Exception (ONLY): VERY short, pure, side‑effect‑free one‑line inline functions in headers (typically simple getters) may be written as:
   `int getCount() const { return count; }`
   Still: no multi statements inside; just a single return or trivial expression. Not allowed for loops or conditionals.
4. No single-line loops: expand body on separate lines.
5. Early returns encouraged for clarity, but must follow rule 1 & 2.

### Statement Density
1. One statement per line (one semicolon terminator per line except for for‑loop headers).
2. Never chain multiple independent statements on a single line.
3. Do not place declarations and logic on the same line after a semicolon.

### Spacing & Formatting
1. Space after control keywords: `if (cond)`, `for (int i = 0; i < n; ++i)`.
2. Spaces around all binary / ternary operators: `a + b * c`, `x = y + 2`, `flag ? on : off`.
3. No space for unary operators: `++i`, `*ptr`, `&ref`.
4. Function calls: no space before `(`: `updateScene(dt);`.
5. 120 column soft limit; break earlier for readability (do not exceed intentionally).
6. Align continued parameters/arguments for clarity; prefer one per line when long.

### Includes
Order groups (blank line between groups):
1. Project headers `"..."`
2. Third-party libs (e.g. `<glm/...>`, `<fmt/...>`, project vendored libs)
3. C / C++ standard library `<...>`
Each group alphabetized; no duplicate includes. Never rely only on transitive includes—include what you use.

### Naming
| Entity | Style |
|--------|-------|
| Types (classes/structs/enums) | PascalCase |
| Functions / methods | camelCase |
| Variables / parameters / members | camelCase |
| Macros / compile-time constants | UPPER_CASE |
| Member variables | m_camelCase |
- Prefer more descriptive variable names over short or single-letter names, especially when the short name has ambiguous meaning. E.g. `SlangCompiler& comp` -> `SlangCompiler& compiler`.
- Still limit variable names to a reasonable length; avoid overly long names that hinder readability.

### Modern C++ Usage
1. Prefer `enum class` over raw `enum`.
2. Prefer `constexpr`, `string_view`, `span`, smart pointers (`unique_ptr` / `shared_ptr`) over owning raw pointers.
3. Explicitly `= delete` / `= default` special members when needed.
4. Mark overrides with `override`.
5. Prefer types defined in src/core/CoreDefs.h over standard library and basic types. (this includes `uint32` over `uint32_t`, `Array` over `std::vector`, `Map` over `std::unordered_map`, etc)

### Error Handling
1. Early return with context string or structured result object; avoid silent failure.
2. No throwing for routine validation; return error code / result struct.

### Comments & Documentation
1. Use `//` for brief comments; reserve block comments only for large explanations or temporarily disabled code.
2. Document non-obvious invariants and performance-sensitive sections.

### Density Examples
DO:
```cpp
if (!buffer) {
    return {};
}
for (auto &path : includePaths) {
    req->addSearchPath(path.c_str());
}
int value() const { return count; } // allowed short inline getter in header
```
AVOID:
```cpp
if(!buffer) return {}; for(auto&p:paths){req->addSearchPath(p.c_str());}
for(auto&p:paths){req->addSearchPath(p.c_str());}
int value() const { if (count < 0) return 0; return count; } // too complex for one line
```

### Prompt Snippet (Use When Asking Copilot)
```
RadiantEngine style: braces always (except trivial one-line header getters), no single-line control statements, one statement per line, spaces around operators, 100-col wrap, naming: Types PascalCase, functions camelCase, variables camelCase, include ordering project / third-party / standard.
```

### Markdown Formatting
Prefer un-numbered headers for sections to make it easier to modify and rearrange content.

### Tooling Alignment
`clang-format` enforce most (braces, spacing, line length). Manual review still required for: multi statements on one line, complexity of inline header getters.
Note: Not run when using vscode, so can't be relied upon for automatic formatting on save. Can be referenced in copilot context for extra guidance.

Adhere strictly; deviations require explicit justification in PR description.
