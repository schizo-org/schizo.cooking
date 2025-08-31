# Marker

This is a somewhat reliable Markdown parser, written in C purely for comedic
factor. Also because I enjoy shooting myself in the foot. Also because C is a
somewhat fun language if you're insane enough.

Marker is designed with _a_ user in mind, but that's probably not _you_. While I
do make an effort to ensure stability and safety, there has been no proper
security audits and it is very likely that I have missed something while
reviewing my own code. Happens to the best of us, you know?

Worst markdown parser on the face of earth, written in C because I enjoy
shooting myself in the foot. Designed with a user in mind, but not for yours.

Please do not try to use Marker in a real production scenario, at least not
unless you are **very** sure that you know what you are doing. You are, however,
very welcome mess around with it and submit PRs to improve functionality. This
project welcomes any contributions

## Features

Marker supports the entirety of CommonMark, and some extensions of the GFM spec.
The exact amount of coverage is deliberate, because my needs are modest. PRs
implementing features that are not yet covered, or improving the

### Core Markdown Support (CommonMark Compliant)

- **Headers**: `# H1` through `###### H6`
- **Emphasis**: `*italic*`, `_italic_`, `**bold**`, `__bold__`
- **Code**: `` `inline code` `` and fenced code blocks
- **Links**: `[text](url)`, `[text](url "title")`, reference links, autolinks
- **Images**: `![alt](url)`, `![alt](url "title")`, reference images
- **Lists**: Unordered (`-`, `*`, `+`) and ordered (`1.`, `2.`, etc.)
- **Blockquotes**: `> quoted text`
- **Horizontal rules**: `---`, `***`, `___`
- **Line breaks**: Hard breaks and paragraph separation
- **HTML entities**: Proper escaping of `&`, `<`, `>`, `"`, `'`
- **Escape sequences**: `\*`, `\_`, etc.

### GitHub Flavored Markdown (GFM) Extensions

- **Strikethrough**: `~~deleted text~~`
- **Tables**: Pipe-separated tables with alignment
- **Task lists**: `- [x] completed` and `- [ ] todo`
- **Autolinks**: Automatic URL and email detection

### Additional Features

- **Inline HTML**: Configurable HTML passthrough support
- **Reference links**: `[text][ref]` with `[ref]: url "title"`
- **Nested emphasis**: Proper handling of complex formatting
- **Multi-backtick code spans**: ``code with ` backtick``
- **HTML escaping**: Configurable entity escaping for security
- **Error recovery**: Graceful handling of malformed input

## Building

First of all, why would you? Secondly, read below.

### Requirements

Marker requires a C99-compatible compiler (e.g., GCC, Clang or MSVC) and
GNUMake. There are no additional dependencies, simply running `make` will build
the library.

## Examples

### Basic Parsing

```c
#include "marker.h"

int main() {
    const char* markdown = "# Hello\nThis is **bold** text.";
    char html[1024];

    marker_result_t result = marker_to_html(markdown, html, sizeof(html), NULL);
    if (result == MARKER_OK) {
        printf("%s\n", html);
    }
    return 0;
}
```

### File Processing

```c
marker_result_t result = marker_file_to_html_file(
    "input.md",
    "output.html",
    "styles.css"
);
```

### Custom Configuration

```c
marker_config_t config;
marker_config_init(&config);
config.enable_tables = false;        // disable tables
config.escape_html = true;           // enable HTML escaping
config.enable_inline_html = false;   // disable raw HTML

marker_parser_t* parser = marker_parser_new(&config);
```

### Reference Links

```c
// Add reference link definitions
marker_add_reference_link(parser, "ref1", "https://example.com", "Example Site");

// Parse markdown with reference
const char* md = "[Click here][ref1]";
marker_parse(parser, md, buffer);
```

## Security Considerations

Marker _is_ designed with security in mind, but it is not impossible to simply
cover everything despite extensive testing. Below are some considerations that
you should keep in mind.

### HTML Escaping

When `escape_html` is enabled, the library automatically escapes dangerous HTML
characters:

- `&` -> `&amp;`
- `<` -> `&lt;`
- `>` -> `&gt;`
- `"` -> `&quot;`
- `'` -> `&#39;`

### Inline HTML Control

The `enable_inline_html` option controls whether raw HTML tags are passed
through or escaped, which _should_ be providing protection against XSS attacks
when disabled. In theory.

### Input Validation

Marker _does_ perform input validation and sanitization to prevent buffer
overflows or similar security issues, though in an ideal scenario you would
parse and sanitize beforehand. Marker is _not_ designed with public use in mind.

## Performance

Besides security, Marker makes an explicit effort for top notch performance.
Namely, it has been designed to be memory efficient and fast. Buffer allocation
is dynamic, but you can configure initial sizes with little compromise. The
single-pass parser is also optimized with minimal backtracing, and scaling is
seamless.

## Compliance

### CommonMark

The library aims for full CommonMark 0.30 compliance, passing the official
CommonMark test suite for:

- Block-level elements (headers, paragraphs, lists, blockquotes, code blocks)
- Inline elements (emphasis, links, images, code spans)
- HTML handling and entity processing
- Edge cases and corner conditions

### GitHub Flavored Markdown

Supports GFM extensions as specified in the GitHub Flavored Markdown
specification:

- Tables with alignment
- Strikethrough text
- Task list items
- Extended autolink support

## Contributing

Marker was not designed as a public library, however, it has ultimately evolved
to accept public contributions. Thus, contributions are very welcome. You may
have a security concern that seems far fetched or a performance improvement that
I've missed. Feel free to submit a PR or an issue to discuss it!

When contributing please remember to:

1. Ensure all tests pass via `make test`.
2. Add new tests for new features
3. Update documentation for any and all changes
4. Consider the security implications of all changes even if it is not a part of
   your use case.

## License

Marker is licensed under Mozilla Public License Version 2.0. Please see
[LICENSE](./LICENSE) for more details.
