# Quickie

<!-- markdownlint-disable MD033 -->
<h6>
  Pronounced like the <a href="https://en.wikipedia.org/wiki/Quiche">the food</a>
</h6>

---

[schizo.cooking]: https://schizo.cooking
[Iris]: ../iris
[Marker]: ../marker

Quickie is the minimalistic glue code and our very own "CMS" for the
[schizo.cooking] project. It wraps and combines the [Iris] webserver, and the
[Marker] markdown renderer to provide a simple and _entirely_ full of shit
static site "generator" that serves our most based takes. The goal is to write
the best worst code ever conceived. Does it work? Yes. _Should_ it work? No.

## What The Hell Does It Do

Quickie is very simple on paper. Enforcing C99 has not made it any more simple,
but it's capabilities are simple at the very least. Quickie can:

- Recursively scan a directory for Markdown (`.md`) files.
- Convert all found Markdown files to HTML using Marker.
- Serve the generated HTML files using the Iris HTTP server optionally with
  dynamic rendering.
- Optionally allow you to separate the source Markdown directory and the
  output/served HTML directory.

While Quickie does support dynamic rendering from a directory, there is **no
upload system**; you should handle uploads yourself if you plan to run Quickie.
This is because Quickie is intended for environments where Markdown files are
managed by other means (e.g., git, rsync, manual copy). If you ask for an
auth-based CMS, then you will be _dropkicked_. Sorry, not sorry.

## Usage

```txt
quickie [-b ADDRESS] [-m MD_DIR] [-o HTML_DIR] [-p PORT]
```

- `-b ADDRESS` Bind address (default: `0.0.0.0`)
- `-m MD_DIR` Directory to scan for Markdown files (default: `.`)
- `-o HTML_DIR` Directory to output and serve HTML files (default: `.`)
- `-p PORT` Port to listen on (default: `8080`)

**Example:**

```sh
./quickie -m ./markdown -o ./public_html -p 8081
```

This will:

1. Scan and watch `./markdown` for file changes and automatically regenerate
   modified files
2. Convert them to HTML in `./public_html` (preserving directory structure)
3. Serve the HTML files at `http://localhost:8081/`

### Live Reload and File Watching

As of December 2025 Quickie uses Linux `inotify` API to watch the Markdown
source directory for changes. This is referred to as _dynamic rendering_ in
documentation and the project source. That said, when a `.md` file is created,
modified, or deleted, Quickie automatically:

- Regenerates the corresponding HTML file for modified/created files
- Deletes the corresponding HTML file when source is removed
- Recursively watches newly created subdirectories

The file watcher runs in a separate thread and operates continuously while the
server is running. No manual intervention or server restart is required to pick
up changes. Simply edit your Markdown files and refresh your browser.

## Additional Notes

Some things worth nothing if you plan to ~~have a quickie~~ run this godforsaken
program on your system.

### Unicode and Non-ASCII Filenames

Quickie uses POSIX APIs (`opendir`, `readdir`, `stat`, etc.) and `char *` for
filenames, which means UTF-8 and non-ASCII filenames are supported as opaque
byte strings on Linux. Thus, there is no ASCII-only logic. Also, **path length
limits are enforced in bytes**, so multi-byte UTF-8 filenames may hit the buffer
limit much sooner. Fret not though; if your locale and filesystem support UTF-8,
Quickie will handle such filenames correctly. Works on my machine.

### Concurrency Safety

Quickie is **not safe for concurrent execution** on the same HTML output
directory. If multiple instances run at the same time, you may get race
conditions or inconsistent output. Always run only one instance at a time per
output directory. Though I don't really know why you would run multiple
instances of this.

## License

Quickie is licensed under Mozilla Public License Version 2.0. Please see
[LICENSE](./LICENSE) for more details.
