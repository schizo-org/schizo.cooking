#define _POSIX_C_SOURCE 200112L
#include "../iris/src/iris.h"
#include "../marker/src/marker.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>

#define QUICKIE_MAX_MD_FILES 1024
#define QUICKIE_MAX_PATH 512
#define QUICKIE_MAX_HTML_SIZE 8192
#define QUICKIE_DEFAULT_PORT 8080
#define QUICKIE_DEFAULT_MD_DIR "."
#define QUICKIE_DEFAULT_HTML_DIR "."

// Log with timestamp and error level
// This could have been a library, but I like this as it is.
static void log_error(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  time_t    now = time(NULL);
  struct tm tm_now;
  localtime_r(&now, &tm_now);

  char timebuf[32];
  strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_now);

  fprintf(stderr, "[%s] [ERROR] ", timebuf);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);
}

// Recursively create directories
static int mkdir_recursive(const char* path, mode_t mode) {
  char   tmp[QUICKIE_MAX_PATH];
  size_t len;

  if (!path)
    return -1;

  len = strlen(path);
  if (len == 0 || len >= QUICKIE_MAX_PATH - 1) {
    log_error("Path too long in mkdir_recursive");
    return -1;
  }

  strncpy(tmp, path, QUICKIE_MAX_PATH - 1);
  tmp[QUICKIE_MAX_PATH - 1] = '\0';

  if (tmp[len - 1] == '/')
    tmp[len - 1] = '\0';

  for (char* p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        return -1;
      }
      *p = '/';
    }
  }
  if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

typedef struct {
  char md_path[QUICKIE_MAX_PATH];
  char html_path[QUICKIE_MAX_PATH];
} quickie_md_html_entry;

static quickie_md_html_entry* quickie_entries        = NULL;
static int                    quickie_entry_count    = 0;
static int                    quickie_entry_capacity = 0;
static char                   quickie_css_file[QUICKIE_MAX_PATH] = "";

#define QUICKIE_INIT_ENTRY_CAPACITY 128

// Ensure quickie_entries is allocated and has space for one more entry
// You could call it a quickfix. Ha.
static int quickie_fix_capacity(void) {
  if (quickie_entry_count >= quickie_entry_capacity) {
    if (quickie_entry_capacity > INT_MAX / 2) {
      log_error("Maximum capacity reached, cannot allocate more entries");
      return 0;
    }

    int new_capacity =
        quickie_entry_capacity > 0 ? quickie_entry_capacity * 2 : QUICKIE_INIT_ENTRY_CAPACITY;

    if (new_capacity > INT_MAX / (int)sizeof(quickie_md_html_entry)) {
      log_error("Allocation would overflow");
      return 0;
    }

    void* new_entries =
        realloc(quickie_entries, (size_t) new_capacity * sizeof(quickie_md_html_entry));
    if (!new_entries) {
      log_error("Failed to allocate memory for markdown file entries (count=%d)", new_capacity);
      return 0;
    }
    quickie_entries        = (quickie_md_html_entry*) new_entries;
    quickie_entry_capacity = new_capacity;
  }
  return 1;
}

// Recursively scan for .md files in the given directory and fill
// `quickie_entries`
void quickie_scan_markdown(const char* md_base_dir, const char* rel_dir) {
  char dir_path[QUICKIE_MAX_PATH];
  int  dir_path_len;

  if (!md_base_dir)
    return;

  if (rel_dir && (strstr(rel_dir, "..") || strstr(rel_dir, "/./"))) {
    log_error("Directory traversal attempt detected: %s", rel_dir);
    return;
  }

  dir_path_len = snprintf(dir_path, sizeof(dir_path), "%s/%s", md_base_dir, rel_dir ? rel_dir : "");
  if (dir_path_len < 0 || dir_path_len >= (int) sizeof(dir_path)) {
    log_error("Directory path too long");
    return;
  }
  DIR* dir = opendir(dir_path);
  if (!dir)
    return;

  struct dirent* entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    if (strlen(entry->d_name) == 0 || strlen(entry->d_name) >= 256) {
      log_error("Invalid filename length: %s", entry->d_name);
      continue;
    }

    if (strstr(entry->d_name, "..") || strchr(entry->d_name, '/')) {
      log_error("Invalid filename characters: %s", entry->d_name);
      continue;
    }

    char rel_path[QUICKIE_MAX_PATH];
    int  rel_path_len;
    if (rel_dir && strlen(rel_dir) > 0) {
      rel_path_len = snprintf(rel_path, sizeof(rel_path), "%s/%s", rel_dir, entry->d_name);
      if (rel_path_len < 0 || rel_path_len >= (int) sizeof(rel_path)) {
        log_error("Relative path too long");
        continue;
      }
    } else {
      rel_path_len = snprintf(rel_path, sizeof(rel_path), "%s", entry->d_name);
      if (rel_path_len < 0 || rel_path_len >= (int) sizeof(rel_path)) {
        log_error("Relative path too long");
        continue;
      }
    }

    char full_path[QUICKIE_MAX_PATH];
    int  full_path_len = snprintf(full_path, sizeof(full_path), "%s/%s", md_base_dir, rel_path);
    if (full_path_len < 0 || full_path_len >= (int) sizeof(full_path)) {
      log_error("Full path too long");
      continue;
    }

    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        quickie_scan_markdown(md_base_dir, rel_path);
      } else if (S_ISREG(st.st_mode)) {
        const char* dot = strrchr(entry->d_name, '.');
        if (dot && strcmp(dot, ".md") == 0) {
          if (!quickie_fix_capacity()) {
            log_error("Failed to allocate space for new markdown entry: %s", rel_path);
            continue;
          }
          int md_path_len = snprintf(quickie_entries[quickie_entry_count].md_path, QUICKIE_MAX_PATH,
                                     "%s", rel_path);
          if (md_path_len < 0 || md_path_len >= QUICKIE_MAX_PATH) {
            log_error("Markdown entry path too long: %s", rel_path);
            continue;
          }

          // Generate HTML path: replace .md with .html (relative path, not
          // full)
          int html_path_len = snprintf(quickie_entries[quickie_entry_count].html_path,
                                       QUICKIE_MAX_PATH, "%s", rel_path);
          if (html_path_len < 0 || html_path_len >= QUICKIE_MAX_PATH) {
            log_error("HTML entry path too long: %s", rel_path);
            continue;
          }
          char* html_ext = strrchr(quickie_entries[quickie_entry_count].html_path, '.');
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
void quickie_convert_all(const char* md_base_dir, const char* html_base_dir, const char* css_file) {
  for (int i = 0; i < quickie_entry_count; ++i) {
    char md_full[QUICKIE_MAX_PATH], html_full[QUICKIE_MAX_PATH];
    int  md_full_len =
        snprintf(md_full, sizeof(md_full), "%s/%s", md_base_dir, quickie_entries[i].md_path);
    if (md_full_len < 0 || md_full_len >= (int) sizeof(md_full)) {
      log_error("Markdown file path too long: %s/%s", md_base_dir, quickie_entries[i].md_path);
      continue;
    }
    int html_full_len = snprintf(html_full, sizeof(html_full), "%s/%s", html_base_dir,
                                 quickie_entries[i].html_path);
    if (html_full_len < 0 || html_full_len >= (int) sizeof(html_full)) {
      log_error("HTML file path too long: %s/%s", html_base_dir, quickie_entries[i].html_path);
      continue;
    }

    // Output dir must exist
    char* slash = strrchr(html_full, '/');
    if (slash) {
      *slash = '\0';
      if (mkdir_recursive(html_full, 0755) != 0) {
        log_error("Failed to create directory: %s (%s)", html_full, strerror(errno));
        *slash = '/';
        continue;
      }
      *slash = '/';
    }

    // Write to temp file first, then (atomically) rename
    // XXX: Or is it atomicly? I don't really know.
    char html_tmp[QUICKIE_MAX_PATH];
    int  html_tmp_len = snprintf(html_tmp, sizeof(html_tmp), "%s.tmp", html_full);
    if (html_tmp_len < 0 || html_tmp_len >= (int) sizeof(html_tmp)) {
      log_error("Temp HTML file path too long: %s.tmp", html_full);
      continue;
    }

    int res = md_file_to_html_file(md_full, html_tmp, css_file);
    if (res == 0) {
      // Atomic rename to final destination
      if (rename(html_tmp, html_full) != 0) {
        log_error("Failed to rename temp file %s to %s: %s", html_tmp, html_full, strerror(errno));
        // Remove temp file if rename fails
        unlink(html_tmp);
        continue;
      }
    } else {
      log_error("Failed to convert %s to HTML (error %d)", md_full, res);
      // Remove temp file if conversion failed
      unlink(html_tmp);
    }
  }
}

int quickie_intercept_markdown(char* path) {
  char* bname = basename(path);
  if (bname) {
    printf("quickie: checking path %s, %s\n", path, bname);
    const char* dot = strrchr(bname, '.');
    if (dot && strcmp(dot, ".html") == 0) {
      char md_full[QUICKIE_MAX_PATH];
      strcpy(md_full, path);
      int len = strlen(path);
      md_full[len-4] = 'm';
      md_full[len-3] = 'd';
      md_full[len-2] = '\0';
      
      // Write to temp file first, then (atomically) rename
      // XXX: Or is it atomicly? I don't really know.
      char html_tmp[QUICKIE_MAX_PATH];
      int  html_tmp_len = snprintf(html_tmp, sizeof(html_tmp), "%s.tmp", path);
      if (html_tmp_len < 0 || html_tmp_len >= (int) sizeof(html_tmp)) {
        log_error("Temp HTML file path too long: %s.tmp", path);
        return 0;
      }
  
      int res = md_file_to_html_file(md_full, html_tmp, quickie_css_file);
      if (res == 0) {
        // Atomic rename to final destination
        if (rename(html_tmp, path) != 0) {
          log_error("Failed to rename temp file %s to %s: %s", html_tmp, path, strerror(errno));
          // Remove temp file if rename fails
          unlink(html_tmp);
          return 0;
        }
      } else {
        log_error("Failed to convert %s to HTML (error %d)", md_full, res);
        // Remove temp file if conversion failed
        unlink(html_tmp);
      }
    }
  }
}

// Serve HTML files using iris, but intercept requests for .md and serve the
// HTML
int quickie_serve(const char* address, const char* md_base_dir, const char* html_base_dir,
                  const char* css_file, int port) {
  if (!address || !md_base_dir || !html_base_dir) {
    log_error("Invalid null parameters");
    return 1;
  }

  if (strlen(address) == 0 || strlen(md_base_dir) == 0 || strlen(html_base_dir) == 0) {
    log_error("Invalid empty parameters");
    return 1;
  }
  // Check if HTML output directory is writable
  struct stat st;
  if (stat(html_base_dir, &st) != 0) {
    log_error("HTML output directory does not exist: %s", html_base_dir);
    return 1;
  }
  if (!S_ISDIR(st.st_mode)) {
    log_error("HTML output path is not a directory: %s", html_base_dir);
    return 1;
  }
  if (access(html_base_dir, W_OK) != 0) {
    log_error("HTML output directory is not writable: %s", html_base_dir);
    return 1;
  }

  return iris_start_with_intercept(address, html_base_dir, port, quickie_intercept_markdown);
}

// Clap solves this
void quickie_usage(const char* prog) {
  fprintf(stderr, "Usage: %s [-b ADDRESS] [-m MD_DIR] [-o HTML_DIR] [-c CSS_FILE] [-p PORT]\n",
          prog);
  fprintf(stderr, "  -b ADDRESS   Bind address (default: 0.0.0.0)\n");
  fprintf(stderr, "  -m MD_DIR    Directory to scan for markdown files (default: .)\n");
  fprintf(stderr, "  -o HTML_DIR  Directory to output and serve HTML files (default: .)\n");
  fprintf(stderr, "  -c CSS_FILE  CSS file to include in HTML output (optional)\n");
  fprintf(stderr, "  -p PORT      Port to listen on (default: %d)\n", QUICKIE_DEFAULT_PORT);
}

int main(int argc, char* argv[]) {
  char address[64]                = "0.0.0.0";
  char md_dir[QUICKIE_MAX_PATH]   = QUICKIE_DEFAULT_MD_DIR;
  char html_dir[QUICKIE_MAX_PATH] = QUICKIE_DEFAULT_HTML_DIR;
  char css_file[QUICKIE_MAX_PATH] = "";
  int  port                       = QUICKIE_DEFAULT_PORT;

  for (int i = 1; i < argc; ++i) {
    if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
      quickie_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
      const char* addr_arg = argv[i + 1];
      if (!addr_arg || strlen(addr_arg) == 0) {
        log_error("Empty address argument");
        return 1;
      }
      if (strlen(addr_arg) >= sizeof(address)) {
        log_error("Address argument too long");
        return 1;
      }
      strncpy(address, argv[++i], sizeof(address) - 1);
      address[sizeof(address) - 1] = '\0';
    } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
      const char* md_arg = argv[i + 1];
      if (!md_arg || strlen(md_arg) == 0) {
        log_error("Empty markdown directory argument");
        return 1;
      }
      if (strlen(md_arg) >= sizeof(md_dir)) {
        log_error("Markdown directory argument too long");
        return 1;
      }
      if (strstr(md_arg, "..")) {
        log_error("Directory traversal detected in markdown directory");
        return 1;
      }
      strncpy(md_dir, argv[++i], sizeof(md_dir) - 1);
      md_dir[sizeof(md_dir) - 1] = '\0';
    } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      const char* html_arg = argv[i + 1];
      if (!html_arg || strlen(html_arg) == 0) {
        log_error("Empty HTML directory argument");
        return 1;
      }
      if (strlen(html_arg) >= sizeof(html_dir)) {
        log_error("HTML directory argument too long");
        return 1;
      }
      if (strstr(html_arg, "..")) {
        log_error("Directory traversal detected in HTML directory");
        return 1;
      }
      strncpy(html_dir, argv[++i], sizeof(html_dir) - 1);
      html_dir[sizeof(html_dir) - 1] = '\0';
    } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
      const char* css_arg = argv[i + 1];
      if (!css_arg || strlen(css_arg) == 0) {
        log_error("Empty CSS file argument");
        return 1;
      }
      if (strlen(css_arg) >= sizeof(css_file)) {
        log_error("CSS file argument too long");
        return 1;
      }
      if (strstr(css_arg, "..")) {
        log_error("Directory traversal detected in CSS file path");
        return 1;
      }
      strncpy(css_file, argv[++i], sizeof(css_file) - 1);
      css_file[sizeof(css_file) - 1] = '\0';
    } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
      const char* port_arg = argv[i + 1];
      if (!port_arg || strlen(port_arg) == 0) {
        log_error("Empty port argument");
        return 1;
      }

      // Validate port is numeric
      for (const char* p = port_arg; *p; p++) {
        if (*p < '0' || *p > '9') {
          log_error("Port must be numeric: %s", port_arg);
          return 1;
        }
      }

      port = atoi(argv[++i]);
      if (port <= 0 || port > 65535) {
        log_error("Invalid port number: %d (must be 1-65535)", port);
        return 1;
      }
    }
  }

  int result =
      quickie_serve(address, md_dir, html_dir, strlen(css_file) > 0 ? css_file : NULL, port);
  free(quickie_entries);
  return result;
}
