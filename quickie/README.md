# Quickie

<!-- markdownlint-disable MD033 -->
<h6>Pronounced like the food</h6>

---

Quickie is a minimal glue "CMS" for the [schizo.cooking](https://schizo.cooking)
project. It combines the [Iris](../iris) web server and the [Marker](../marker)
markdown renderer to provide a simple, full of shit static site generator and
server for Markdown content. The goal is to write the worst code ever conceived.
Does it work? Yes. Should it work? No.

## What does it do?

- Recursively scans a directory for Markdown (`.md`) files.
- Converts all found Markdown files to HTML using Marker.
- Serves the generated HTML files using the Iris HTTP server.
- Optionally allows you to separate the source Markdown directory and the
  output/served HTML directory.

There is **no upload system**. Quickie is intended for environments where
Markdown files are managed by other means (e.g., git, rsync, manual copy). If
you ask for an auth-based CMS, then you will be dropkicked. Sorry, I make the
rules.

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

- Scan `./markdown` for `.md` files
- Convert them to HTML in `./public_html` (preserving directory structure)
- Serve the HTML files at `http://localhost:8081/`

## Additional Notes

Those only apply if you plan to ~~have a quickie~~ run this program on your
system.

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
