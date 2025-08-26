#include "../iris/src/iris.h"
#include "../marker/src/marker.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define QUICKIE_MAX_MD_FILES 1024
#define QUICKIE_MAX_PATH 512
#define QUICKIE_MAX_HTML_SIZE 8192
#define QUICKIE_DEFAULT_PORT 8080
#define QUICKIE_DEFAULT_MD_DIR "."
#define QUICKIE_DEFAULT_HTML_DIR "."

typedef struct {
  char md_path[QUICKIE_MAX_PATH];
  char html_path[QUICKIE_MAX_PATH];
} quickie_md_html_entry;

static quickie_md_html_entry quickie_entries[QUICKIE_MAX_MD_FILES];
static int quickie_entry_count = 0;

// Recursively scan for .md files in the given directory and fill
// quickie_entries
void quickie_scan_markdown(const char *md_base_dir, const char *rel_dir) {
  char dir_path[QUICKIE_MAX_PATH];
  snprintf(dir_path, sizeof(dir_path), "%s/%s", md_base_dir,
           rel_dir ? rel_dir : "");
  DIR *dir = opendir(dir_path);
  if (!dir)
    return;

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    char rel_path[QUICKIE_MAX_PATH];
    if (rel_dir && strlen(rel_dir) > 0)
      snprintf(rel_path, sizeof(rel_path), "%s/%s", rel_dir, entry->d_name);
    else
      snprintf(rel_path, sizeof(rel_path), "%s", entry->d_name);

    char full_path[QUICKIE_MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", md_base_dir, rel_path);

    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        quickie_scan_markdown(md_base_dir, rel_path);
      } else if (S_ISREG(st.st_mode)) {
        const char *dot = strrchr(entry->d_name, '.');
        if (dot && strcmp(dot, ".md") == 0 &&
            quickie_entry_count < QUICKIE_MAX_MD_FILES) {
          snprintf(quickie_entries[quickie_entry_count].md_path,
                   QUICKIE_MAX_PATH, "%s", rel_path);

          // Generate HTML path: replace .md with .html (relative path, not
          // full)
          snprintf(quickie_entries[quickie_entry_count].html_path,
                   QUICKIE_MAX_PATH, "%s", rel_path);
          char *html_ext =
              strrchr(quickie_entries[quickie_entry_count].html_path, '.');
          if (html_ext)
            strcpy(html_ext, ".html");

          quickie_entry_count++;
        }
      }
    }
  }
  closedir(dir);
}

// Convert all found markdown files to HTML using marker
void quickie_convert_all(const char *md_base_dir, const char *html_base_dir,
                         const char *css_file) {
  for (int i = 0; i < quickie_entry_count; ++i) {
    char md_full[QUICKIE_MAX_PATH], html_full[QUICKIE_MAX_PATH];
    snprintf(md_full, sizeof(md_full), "%s/%s", md_base_dir,
             quickie_entries[i].md_path);
    snprintf(html_full, sizeof(html_full), "%s/%s", html_base_dir,
             quickie_entries[i].html_path);

    // Output dir must exist
    char *slash = strrchr(html_full, '/');
    if (slash) {
      *slash = '\0';
      char mkdir_cmd[QUICKIE_MAX_PATH + 32];
      snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", html_full);
      system(mkdir_cmd);
      *slash = '/';
    }

    int res = md_file_to_html_file(md_full, html_full, css_file);
    if (res != 0) {
      fprintf(stderr, "Failed to convert %s to HTML (error %d)\n", md_full,
              res);
    }
  }
}

// Serve HTML files using iris, but intercept requests for .md and serve the
// HTML
int quickie_serve(const char *address, const char *md_base_dir,
                  const char *html_base_dir, int port) {
  // Pre-convert all markdown files to HTML
  quickie_scan_markdown(md_base_dir, NULL);
  quickie_convert_all(md_base_dir, html_base_dir, NULL);

  return iris_start(address, html_base_dir, port);
}

// Clap solves this
void quickie_usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s [-b ADDRESS] [-m MD_DIR] [-o HTML_DIR] [-p PORT]\n", prog);
  fprintf(stderr, "  -b ADDRESS   Bind address (default: 0.0.0.0)\n");
  fprintf(stderr,
          "  -m MD_DIR    Directory to scan for markdown files (default: .)\n");
  fprintf(
      stderr,
      "  -o HTML_DIR  Directory to output and serve HTML files (default: .)\n");
  fprintf(stderr, "  -p PORT      Port to listen on (default: %d)\n",
          QUICKIE_DEFAULT_PORT);
}

int main(int argc, char *argv[]) {
  char address[64] = "0.0.0.0";
  char md_dir[QUICKIE_MAX_PATH] = QUICKIE_DEFAULT_MD_DIR;
  char html_dir[QUICKIE_MAX_PATH] = QUICKIE_DEFAULT_HTML_DIR;
  int port = QUICKIE_DEFAULT_PORT;

  for (int i = 1; i < argc; ++i) {
    if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
      quickie_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
      strncpy(address, argv[++i], sizeof(address) - 1);
    } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
      strncpy(md_dir, argv[++i], sizeof(md_dir) - 1);
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      strncpy(html_dir, argv[++i], sizeof(html_dir) - 1);
    } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
      port = atoi(argv[++i]);
    }
  }

  return quickie_serve(address, md_dir, html_dir, port);
}
