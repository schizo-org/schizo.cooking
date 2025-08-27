#include "marker.h"
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Internal constants */
#define DEFAULT_BUFFER_SIZE 4096
#define MAX_NESTING_DEPTH 32
#define MAX_LINE_LENGTH 4096
#define MAX_LINK_LENGTH 2048

/* Parser state structure */
struct marker_parser {
  marker_config_t    config;
  marker_ref_link_t* ref_links;
  size_t             nesting_depth;
  bool               in_code_block;
  bool               in_html_block;
  char*              line_buffer;
  size_t             line_buffer_size;
};

/* Forward declarations */
static marker_result_t parse_inline_content(marker_parser_t* parser, const char* text, size_t* pos,
                                            marker_buffer_t* output, size_t end_pos);

/* Compatibility function for strcasecmp */
#ifndef strcasecmp
static int strcasecmp(const char* s1, const char* s2) {
  while (*s1 && *s2) {
    int c1 = tolower((unsigned char) *s1);
    int c2 = tolower((unsigned char) *s2);
    if (c1 != c2)
      return c1 - c2;
    s1++;
    s2++;
  }
  return tolower((unsigned char) *s1) - tolower((unsigned char) *s2);
}
#endif

/* HTML entities for escaping */
static const struct {
  char        ch;
  const char* entity;
} html_entities[] = {
    {'&',  "&amp;" },
    {'<',  "&lt;"  },
    {'>',  "&gt;"  },
    {'"',  "&quot;"},
    {'\'', "&#39;" },
    {0,    NULL    }
};

/* Version and error handling */
const char* marker_version(void) { return MARKER_VERSION_STRING; }

const char* marker_error_string(marker_result_t result) {
  switch (result) {
    case MARKER_OK:
      return "Success";
    case MARKER_ERROR_NULL_POINTER:
      return "Null pointer argument";
    case MARKER_ERROR_INVALID_SIZE:
      return "Invalid size argument";
    case MARKER_ERROR_BUFFER_TOO_SMALL:
      return "Output buffer too small";
    case MARKER_ERROR_IO_FAILED:
      return "I/O operation failed";
    case MARKER_ERROR_MEMORY_ALLOCATION:
      return "Memory allocation failed";
    case MARKER_ERROR_INVALID_INPUT:
      return "Invalid input";
    case MARKER_ERROR_PARSE_FAILED:
      return "Parse failed";
    default:
      return "Unknown error";
  }
}

/* Configuration */
void marker_config_init(marker_config_t* config) {
  if (!config)
    return;

  config->enable_tables        = true;
  config->enable_strikethrough = true;
  config->enable_task_lists    = true;
  config->enable_autolinks     = true;
  config->enable_inline_html   = true;
  config->escape_html          = true;
  config->smart_quotes         = false;
  config->hard_line_breaks     = false;
  config->max_nesting_depth    = MAX_NESTING_DEPTH;
  config->initial_buffer_size  = DEFAULT_BUFFER_SIZE;
}

/* Buffer management */
marker_buffer_t* marker_buffer_new(size_t initial_capacity) {
  if (initial_capacity == 0)
    initial_capacity = DEFAULT_BUFFER_SIZE;

  marker_buffer_t* buffer = malloc(sizeof(marker_buffer_t));
  if (!buffer)
    return NULL;

  buffer->data = malloc(initial_capacity);
  if (!buffer->data) {
    free(buffer);
    return NULL;
  }

  buffer->size     = 0;
  buffer->capacity = initial_capacity;
  buffer->data[0]  = '\0';

  return buffer;
}

void marker_buffer_free(marker_buffer_t* buffer) {
  if (!buffer)
    return;
  free(buffer->data);
  free(buffer);
}

const char* marker_buffer_data(const marker_buffer_t* buffer) {
  return buffer ? buffer->data : NULL;
}

size_t marker_buffer_size(const marker_buffer_t* buffer) { return buffer ? buffer->size : 0; }

static marker_result_t buffer_ensure_capacity(marker_buffer_t* buffer, size_t needed) {
  if (!buffer)
    return MARKER_ERROR_NULL_POINTER;

  if (buffer->capacity >= needed)
    return MARKER_OK;

  size_t new_capacity = buffer->capacity;
  while (new_capacity < needed) {
    new_capacity *= 2;
  }

  char* new_data = realloc(buffer->data, new_capacity);
  if (!new_data)
    return MARKER_ERROR_MEMORY_ALLOCATION;

  buffer->data     = new_data;
  buffer->capacity = new_capacity;
  return MARKER_OK;
}

static marker_result_t buffer_append(marker_buffer_t* buffer, const char* text, size_t len) {
  if (!buffer || !text)
    return MARKER_ERROR_NULL_POINTER;
  if (len == 0)
    return MARKER_OK;

  marker_result_t result = buffer_ensure_capacity(buffer, buffer->size + len + 1);
  if (result != MARKER_OK)
    return result;

  memcpy(buffer->data + buffer->size, text, len);
  buffer->size += len;
  buffer->data[buffer->size] = '\0';

  return MARKER_OK;
}

static marker_result_t buffer_append_str(marker_buffer_t* buffer, const char* text) {
  return buffer_append(buffer, text, strlen(text));
}

static marker_result_t buffer_append_char(marker_buffer_t* buffer, char ch) {
  return buffer_append(buffer, &ch, 1);
}

/* Parser management */
marker_parser_t* marker_parser_new(const marker_config_t* config) {
  marker_parser_t* parser = malloc(sizeof(marker_parser_t));
  if (!parser)
    return NULL;

  if (config) {
    parser->config = *config;
  } else {
    marker_config_init(&parser->config);
  }

  parser->ref_links        = NULL;
  parser->nesting_depth    = 0;
  parser->in_code_block    = false;
  parser->in_html_block    = false;
  parser->line_buffer_size = MAX_LINE_LENGTH;
  parser->line_buffer      = malloc(parser->line_buffer_size);

  if (!parser->line_buffer) {
    free(parser);
    return NULL;
  }

  return parser;
}

void marker_parser_free(marker_parser_t* parser) {
  if (!parser)
    return;

  marker_clear_reference_links(parser);
  free(parser->line_buffer);
  free(parser);
}

/* Reference link management */
marker_result_t marker_add_reference_link(marker_parser_t* parser, const char* label,
                                          const char* url, const char* title) {
  if (!parser || !label || !url)
    return MARKER_ERROR_NULL_POINTER;

  marker_ref_link_t* ref = malloc(sizeof(marker_ref_link_t));
  if (!ref)
    return MARKER_ERROR_MEMORY_ALLOCATION;

  ref->label = malloc(strlen(label) + 1);
  ref->url   = malloc(strlen(url) + 1);
  ref->title = title ? malloc(strlen(title) + 1) : NULL;

  if (!ref->label || !ref->url || (title && !ref->title)) {
    free(ref->label);
    free(ref->url);
    free(ref->title);
    free(ref);
    return MARKER_ERROR_MEMORY_ALLOCATION;
  }

  strcpy(ref->label, label);
  strcpy(ref->url, url);
  if (title)
    strcpy(ref->title, title);

  ref->next         = parser->ref_links;
  parser->ref_links = ref;

  return MARKER_OK;
}

void marker_clear_reference_links(marker_parser_t* parser) {
  if (!parser)
    return;

  marker_ref_link_t* current = parser->ref_links;
  while (current) {
    marker_ref_link_t* next = current->next;
    free(current->label);
    free(current->url);
    free(current->title);
    free(current);
    current = next;
  }
  parser->ref_links = NULL;
}

static marker_ref_link_t* find_reference_link(marker_parser_t* parser, const char* label) {
  if (!parser || !label)
    return NULL;

  marker_ref_link_t* current = parser->ref_links;
  while (current) {
    if (strcasecmp(current->label, label) == 0) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

/* HTML escaping */
marker_result_t marker_escape_html(const char* text, char* output, size_t output_size) {
  if (!text || !output || output_size == 0)
    return MARKER_ERROR_NULL_POINTER;

  size_t out_pos = 0;
  size_t in_pos  = 0;

  while (text[in_pos] && out_pos < output_size - 1) {
    bool found = false;

    for (int i = 0; html_entities[i].ch; i++) {
      if (text[in_pos] == html_entities[i].ch) {
        size_t entity_len = strlen(html_entities[i].entity);
        if (out_pos + entity_len >= output_size - 1) {
          return MARKER_ERROR_BUFFER_TOO_SMALL;
        }
        strcpy(output + out_pos, html_entities[i].entity);
        out_pos += entity_len;
        found = true;
        break;
      }
    }

    if (!found) {
      output[out_pos++] = text[in_pos];
    }
    in_pos++;
  }

  output[out_pos] = '\0';
  return text[in_pos] ? MARKER_ERROR_BUFFER_TOO_SMALL : MARKER_OK;
}

static marker_result_t append_escaped_html(marker_buffer_t* buffer, const char* text, size_t len) {
  if (!buffer || !text)
    return MARKER_ERROR_NULL_POINTER;

  for (size_t i = 0; i < len; i++) {
    bool found = false;
    for (int j = 0; html_entities[j].ch; j++) {
      if (text[i] == html_entities[j].ch) {
        marker_result_t result = buffer_append_str(buffer, html_entities[j].entity);
        if (result != MARKER_OK)
          return result;
        found = true;
        break;
      }
    }
    if (!found) {
      marker_result_t result = buffer_append_char(buffer, text[i]);
      if (result != MARKER_OK)
        return result;
    }
  }
  return MARKER_OK;
}

/* Utility functions */
static bool is_whitespace(char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; }

static bool is_punctuation(char ch) { return ispunct((unsigned char) ch); }

static void trim_whitespace(char* str) {
  if (!str)
    return;

  /* Trim leading whitespace */
  char* start = str;
  while (is_whitespace(*start))
    start++;

  if (start != str) {
    memmove(str, start, strlen(start) + 1);
  }

  /* Trim trailing whitespace */
  size_t len = strlen(str);
  while (len > 0 && is_whitespace(str[len - 1])) {
    str[--len] = '\0';
  }
}

static bool starts_with(const char* str, const char* prefix) {
  if (!str || !prefix)
    return false;
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

/* Inline parsing functions */
static size_t find_emphasis_end(const char* text, size_t start, char marker, int count) {
  size_t pos = start + count;

  while (text[pos]) {
    if (text[pos] == marker) {
      int    found_count = 0;
      size_t check_pos   = pos;

      while (text[check_pos] == marker && found_count < count) {
        found_count++;
        check_pos++;
      }

      if (found_count == count) {
        /* Check that it's not just whitespace before */
        if (pos > start + count) {
          char prev = text[pos - 1];
          if (!is_whitespace(prev)) {
            return pos;
          }
        }
      }
    }
    pos++;
  }

  return SIZE_MAX; /* Not found */
}

static marker_result_t parse_link(marker_parser_t* parser, const char* text, size_t* pos,
                                  marker_buffer_t* output) {
  size_t start = *pos;

  if (text[start] != '[')
    return MARKER_ERROR_INVALID_INPUT;

  /* Find the closing bracket for link text */
  size_t text_start    = start + 1;
  size_t text_end      = text_start;
  int    bracket_count = 0;

  while (text[text_end]) {
    if (text[text_end] == '[') {
      bracket_count++;
    } else if (text[text_end] == ']') {
      if (bracket_count == 0)
        break;
      bracket_count--;
    }
    text_end++;
  }

  if (text[text_end] != ']')
    return MARKER_ERROR_INVALID_INPUT;

  /* Check for immediate URL in parentheses */
  if (text[text_end + 1] == '(') {
    size_t url_start = text_end + 2;
    size_t url_end   = url_start;

    /* Find closing parenthesis */
    while (text[url_end] && text[url_end] != ')') {
      url_end++;
    }

    if (text[url_end] != ')')
      return MARKER_ERROR_INVALID_INPUT;

    /* Extract URL and optional title */
    char url[MAX_LINK_LENGTH];
    char title[MAX_LINK_LENGTH] = {0};

    size_t url_len = url_end - url_start;
    if (url_len >= MAX_LINK_LENGTH)
      return MARKER_ERROR_INVALID_INPUT;

    memcpy(url, text + url_start, url_len);
    url[url_len] = '\0';
    trim_whitespace(url);

    /* Check for title in quotes */
    char* title_start = strchr(url, '"');
    if (title_start) {
      *title_start = '\0';
      title_start++;
      char* title_end = strrchr(title_start, '"');
      if (title_end) {
        *title_end = '\0';
        strcpy(title, title_start);
      }
      trim_whitespace(url);
    }

    /* Generate HTML */
    marker_result_t result = buffer_append_str(output, "<a href=\"");
    if (result != MARKER_OK)
      return result;

    if (parser->config.escape_html) {
      result = append_escaped_html(output, url, strlen(url));
    } else {
      result = buffer_append_str(output, url);
    }
    if (result != MARKER_OK)
      return result;

    result = buffer_append_str(output, "\"");
    if (result != MARKER_OK)
      return result;

    if (title[0]) {
      result = buffer_append_str(output, " title=\"");
      if (result != MARKER_OK)
        return result;

      if (parser->config.escape_html) {
        result = append_escaped_html(output, title, strlen(title));
      } else {
        result = buffer_append_str(output, title);
      }
      if (result != MARKER_OK)
        return result;

      result = buffer_append_str(output, "\"");
      if (result != MARKER_OK)
        return result;
    }

    result = buffer_append_str(output, ">");
    if (result != MARKER_OK)
      return result;

    /* Parse inline content of link text */
    size_t link_text_len = text_end - text_start;
    char*  link_text     = malloc(link_text_len + 1);
    if (!link_text)
      return MARKER_ERROR_MEMORY_ALLOCATION;

    memcpy(link_text, text + text_start, link_text_len);
    link_text[link_text_len] = '\0';

    size_t link_pos = 0;
    result          = parse_inline_content(parser, link_text, &link_pos, output, link_text_len);
    free(link_text);
    if (result != MARKER_OK)
      return result;

    result = buffer_append_str(output, "</a>");
    if (result != MARKER_OK)
      return result;

    *pos = url_end + 1;
    return MARKER_OK;
  }

  /* Check for reference link */
  size_t ref_start = text_end + 1;
  if (text[ref_start] == '[') {
    size_t ref_end = ref_start + 1;
    while (text[ref_end] && text[ref_end] != ']') {
      ref_end++;
    }

    if (text[ref_end] == ']') {
      char   ref_label[MAX_LINK_LENGTH];
      size_t ref_len = ref_end - ref_start - 1;

      if (ref_len == 0) {
        /* Empty reference - use link text as reference */
        ref_len = text_end - text_start;
        if (ref_len >= MAX_LINK_LENGTH)
          return MARKER_ERROR_INVALID_INPUT;
        memcpy(ref_label, text + text_start, ref_len);
      } else {
        if (ref_len >= MAX_LINK_LENGTH)
          return MARKER_ERROR_INVALID_INPUT;
        memcpy(ref_label, text + ref_start + 1, ref_len);
      }

      ref_label[ref_len] = '\0';
      trim_whitespace(ref_label);

      marker_ref_link_t* ref = find_reference_link(parser, ref_label);
      if (ref) {
        /* Generate HTML for reference link */
        marker_result_t result = buffer_append_str(output, "<a href=\"");
        if (result != MARKER_OK)
          return result;

        if (parser->config.escape_html) {
          result = append_escaped_html(output, ref->url, strlen(ref->url));
        } else {
          result = buffer_append_str(output, ref->url);
        }
        if (result != MARKER_OK)
          return result;

        result = buffer_append_str(output, "\"");
        if (result != MARKER_OK)
          return result;

        if (ref->title) {
          result = buffer_append_str(output, " title=\"");
          if (result != MARKER_OK)
            return result;

          if (parser->config.escape_html) {
            result = append_escaped_html(output, ref->title, strlen(ref->title));
          } else {
            result = buffer_append_str(output, ref->title);
          }
          if (result != MARKER_OK)
            return result;

          result = buffer_append_str(output, "\"");
          if (result != MARKER_OK)
            return result;
        }

        result = buffer_append_str(output, ">");
        if (result != MARKER_OK)
          return result;

        /* Parse link text */
        size_t link_text_len = text_end - text_start;
        char*  link_text     = malloc(link_text_len + 1);
        if (!link_text)
          return MARKER_ERROR_MEMORY_ALLOCATION;

        memcpy(link_text, text + text_start, link_text_len);
        link_text[link_text_len] = '\0';

        size_t link_pos = 0;
        result          = parse_inline_content(parser, link_text, &link_pos, output, link_text_len);
        free(link_text);
        if (result != MARKER_OK)
          return result;

        result = buffer_append_str(output, "</a>");
        if (result != MARKER_OK)
          return result;

        *pos = ref_end + 1;
        return MARKER_OK;
      }
    }
  }

  /* Check for shortcut reference link [link text] */
  char   ref_label[MAX_LINK_LENGTH];
  size_t link_text_len = text_end - text_start;
  if (link_text_len < MAX_LINK_LENGTH) {
    memcpy(ref_label, text + text_start, link_text_len);
    ref_label[link_text_len] = '\0';
    trim_whitespace(ref_label);

    marker_ref_link_t* ref = find_reference_link(parser, ref_label);
    if (ref) {
      /* Generate HTML for shortcut reference link */
      marker_result_t result = buffer_append_str(output, "<a href=\"");
      if (result != MARKER_OK)
        return result;

      if (parser->config.escape_html) {
        result = append_escaped_html(output, ref->url, strlen(ref->url));
      } else {
        result = buffer_append_str(output, ref->url);
      }
      if (result != MARKER_OK)
        return result;

      result = buffer_append_str(output, "\"");
      if (result != MARKER_OK)
        return result;

      if (ref->title) {
        result = buffer_append_str(output, " title=\"");
        if (result != MARKER_OK)
          return result;

        if (parser->config.escape_html) {
          result = append_escaped_html(output, ref->title, strlen(ref->title));
        } else {
          result = buffer_append_str(output, ref->title);
        }
        if (result != MARKER_OK)
          return result;

        result = buffer_append_str(output, "\"");
        if (result != MARKER_OK)
          return result;
      }

      result = buffer_append_str(output, ">");
      if (result != MARKER_OK)
        return result;

      /* Parse link text */
      char* link_text = malloc(link_text_len + 1);
      if (!link_text)
        return MARKER_ERROR_MEMORY_ALLOCATION;

      memcpy(link_text, text + text_start, link_text_len);
      link_text[link_text_len] = '\0';

      size_t link_pos = 0;
      result          = parse_inline_content(parser, link_text, &link_pos, output, link_text_len);
      free(link_text);
      if (result != MARKER_OK)
        return result;

      result = buffer_append_str(output, "</a>");
      if (result != MARKER_OK)
        return result;

      *pos = text_end + 1;
      return MARKER_OK;
    }
  }

  return MARKER_ERROR_INVALID_INPUT;
}

static marker_result_t parse_image(marker_parser_t* parser, const char* text, size_t* pos,
                                   marker_buffer_t* output) {
  size_t start = *pos;

  if (text[start] != '!' || text[start + 1] != '[')
    return MARKER_ERROR_INVALID_INPUT;

  /* Find the closing bracket for alt text */
  size_t alt_start = start + 2;
  size_t alt_end   = alt_start;

  while (text[alt_end] && text[alt_end] != ']') {
    alt_end++;
  }

  if (text[alt_end] != ']')
    return MARKER_ERROR_INVALID_INPUT;

  /* Check for immediate URL in parentheses */
  if (text[alt_end + 1] == '(') {
    size_t url_start = alt_end + 2;
    size_t url_end   = url_start;

    while (text[url_end] && text[url_end] != ')') {
      url_end++;
    }

    if (text[url_end] != ')')
      return MARKER_ERROR_INVALID_INPUT;

    /* Extract URL and optional title */
    char url[MAX_LINK_LENGTH];
    char title[MAX_LINK_LENGTH] = {0};
    char alt[MAX_LINK_LENGTH];

    size_t url_len = url_end - url_start;
    size_t alt_len = alt_end - alt_start;

    if (url_len >= MAX_LINK_LENGTH || alt_len >= MAX_LINK_LENGTH) {
      return MARKER_ERROR_INVALID_INPUT;
    }

    memcpy(url, text + url_start, url_len);
    url[url_len] = '\0';
    trim_whitespace(url);

    memcpy(alt, text + alt_start, alt_len);
    alt[alt_len] = '\0';

    /* Check for title in quotes */
    char* title_start = strchr(url, '"');
    if (title_start) {
      *title_start = '\0';
      title_start++;
      char* title_end = strrchr(title_start, '"');
      if (title_end) {
        *title_end = '\0';
        strcpy(title, title_start);
      }
      trim_whitespace(url);
    }

    /* Generate HTML */
    marker_result_t result = buffer_append_str(output, "<img src=\"");
    if (result != MARKER_OK)
      return result;

    if (parser->config.escape_html) {
      result = append_escaped_html(output, url, strlen(url));
    } else {
      result = buffer_append_str(output, url);
    }
    if (result != MARKER_OK)
      return result;

    result = buffer_append_str(output, "\" alt=\"");
    if (result != MARKER_OK)
      return result;

    if (parser->config.escape_html) {
      result = append_escaped_html(output, alt, strlen(alt));
    } else {
      result = buffer_append_str(output, alt);
    }
    if (result != MARKER_OK)
      return result;

    result = buffer_append_str(output, "\"");
    if (result != MARKER_OK)
      return result;

    if (title[0]) {
      result = buffer_append_str(output, " title=\"");
      if (result != MARKER_OK)
        return result;

      if (parser->config.escape_html) {
        result = append_escaped_html(output, title, strlen(title));
      } else {
        result = buffer_append_str(output, title);
      }
      if (result != MARKER_OK)
        return result;

      result = buffer_append_str(output, "\"");
      if (result != MARKER_OK)
        return result;
    }

    result = buffer_append_str(output, ">");
    if (result != MARKER_OK)
      return result;

    *pos = url_end + 1;
    return MARKER_OK;
  }

  return MARKER_ERROR_INVALID_INPUT;
}

static marker_result_t parse_autolink(const char* text, size_t* pos, marker_buffer_t* output) {
  size_t start = *pos;

  if (text[start] != '<')
    return MARKER_ERROR_INVALID_INPUT;

  size_t end = start + 1;
  while (text[end] && text[end] != '>' && text[end] != ' ' && text[end] != '\n') {
    end++;
  }

  if (text[end] != '>')
    return MARKER_ERROR_INVALID_INPUT;

  size_t content_len = end - start - 1;
  char*  content     = malloc(content_len + 1);
  if (!content)
    return MARKER_ERROR_MEMORY_ALLOCATION;

  memcpy(content, text + start + 1, content_len);
  content[content_len] = '\0';

  bool is_email = strchr(content, '@') != NULL;
  bool is_url   = starts_with(content, "http://") || starts_with(content, "https://") ||
                starts_with(content, "ftp://");

  if (is_email || is_url) {
    marker_result_t result = buffer_append_str(output, "<a href=\"");
    if (result != MARKER_OK) {
      free(content);
      return result;
    }

    if (is_email) {
      result = buffer_append_str(output, "mailto:");
      if (result != MARKER_OK) {
        free(content);
        return result;
      }
    }

    result = buffer_append_str(output, content);
    if (result != MARKER_OK) {
      free(content);
      return result;
    }

    result = buffer_append_str(output, "\">");
    if (result != MARKER_OK) {
      free(content);
      return result;
    }

    result = buffer_append_str(output, content);
    if (result != MARKER_OK) {
      free(content);
      return result;
    }

    result = buffer_append_str(output, "</a>");
    free(content);
    if (result != MARKER_OK)
      return result;

    *pos = end + 1;
    return MARKER_OK;
  }

  free(content);
  return MARKER_ERROR_INVALID_INPUT;
}

static marker_result_t parse_code_span(const char* text, size_t* pos, marker_buffer_t* output) {
  size_t start = *pos;

  if (text[start] != '`')
    return MARKER_ERROR_INVALID_INPUT;

  /* Count opening backticks */
  size_t tick_count = 0;
  while (text[start + tick_count] == '`') {
    tick_count++;
  }

  /* Find matching closing backticks */
  size_t content_start = start + tick_count;
  size_t content_end   = content_start;

  while (text[content_end]) {
    if (text[content_end] == '`') {
      size_t closing_ticks = 0;
      size_t check_pos     = content_end;

      while (text[check_pos] == '`' && closing_ticks < tick_count) {
        closing_ticks++;
        check_pos++;
      }

      if (closing_ticks == tick_count) {
        /* Found matching closing backticks */
        marker_result_t result = buffer_append_str(output, "<code>");
        if (result != MARKER_OK)
          return result;

        /* Trim one space from each end if present */
        size_t trim_start = content_start;
        size_t trim_end   = content_end;

        if (trim_end > trim_start && text[trim_start] == ' ') {
          trim_start++;
        }
        if (trim_end > trim_start && text[trim_end - 1] == ' ') {
          trim_end--;
        }

        /* Escape HTML in code content */
        for (size_t i = trim_start; i < trim_end; i++) {
          bool found = false;
          for (int j = 0; html_entities[j].ch; j++) {
            if (text[i] == html_entities[j].ch) {
              result = buffer_append_str(output, html_entities[j].entity);
              if (result != MARKER_OK)
                return result;
              found = true;
              break;
            }
          }
          if (!found) {
            result = buffer_append_char(output, text[i]);
            if (result != MARKER_OK)
              return result;
          }
        }

        result = buffer_append_str(output, "</code>");
        if (result != MARKER_OK)
          return result;

        *pos = content_end + tick_count;
        return MARKER_OK;
      }
    }
    content_end++;
  }

  return MARKER_ERROR_INVALID_INPUT;
}

static marker_result_t parse_inline_content(marker_parser_t* parser, const char* text, size_t* pos,
                                            marker_buffer_t* output, size_t end_pos) {
  if (!parser || !text || !pos || !output)
    return MARKER_ERROR_NULL_POINTER;

  while (*pos < end_pos && text[*pos]) {
    char ch = text[*pos];

    /* Handle escape sequences */
    if (ch == '\\' && text[*pos + 1]) {
      char next = text[*pos + 1];
      if (is_punctuation(next)) {
        marker_result_t result = buffer_append_char(output, next);
        if (result != MARKER_OK)
          return result;
        *pos += 2;
        continue;
      }
    }

    /* Handle emphasis and strong */
    if (ch == '*' || ch == '_') {
      bool is_strong = (text[*pos + 1] == ch);
      int  count     = is_strong ? 2 : 1;

      size_t end = find_emphasis_end(text, *pos, ch, count);
      if (end != SIZE_MAX) {
        const char* tag = is_strong ? "strong" : "em";

        marker_result_t result = buffer_append_str(output, "<");
        if (result != MARKER_OK)
          return result;
        result = buffer_append_str(output, tag);
        if (result != MARKER_OK)
          return result;
        result = buffer_append_str(output, ">");
        if (result != MARKER_OK)
          return result;

        size_t content_pos = *pos + count;
        result             = parse_inline_content(parser, text, &content_pos, output, end);
        if (result != MARKER_OK)
          return result;

        result = buffer_append_str(output, "</");
        if (result != MARKER_OK)
          return result;
        result = buffer_append_str(output, tag);
        if (result != MARKER_OK)
          return result;
        result = buffer_append_str(output, ">");
        if (result != MARKER_OK)
          return result;

        *pos = end + count;
        continue;
      }
    }

    /* Handle strikethrough */
    if (parser->config.enable_strikethrough && ch == '~' && text[*pos + 1] == '~') {
      size_t end = *pos + 2;
      while (text[end] && !(text[end] == '~' && text[end + 1] == '~')) {
        end++;
      }

      if (text[end] == '~' && text[end + 1] == '~') {
        marker_result_t result = buffer_append_str(output, "<del>");
        if (result != MARKER_OK)
          return result;

        size_t content_pos = *pos + 2;
        result             = parse_inline_content(parser, text, &content_pos, output, end);
        if (result != MARKER_OK)
          return result;

        result = buffer_append_str(output, "</del>");
        if (result != MARKER_OK)
          return result;

        *pos = end + 2;
        continue;
      }
    }

    /* Handle code spans */
    if (ch == '`') {
      size_t          old_pos = *pos;
      marker_result_t result  = parse_code_span(text, pos, output);
      if (result == MARKER_OK)
        continue;
      *pos = old_pos;
    }

    /* Handle images */
    if (ch == '!' && text[*pos + 1] == '[') {
      size_t          old_pos = *pos;
      marker_result_t result  = parse_image(parser, text, pos, output);
      if (result == MARKER_OK)
        continue;
      *pos = old_pos;
    }

    /* Handle links */
    if (ch == '[') {
      size_t          old_pos = *pos;
      marker_result_t result  = parse_link(parser, text, pos, output);
      if (result == MARKER_OK)
        continue;
      *pos = old_pos;
    }

    /* Handle autolinks */
    if (parser->config.enable_autolinks && ch == '<') {
      size_t          old_pos = *pos;
      marker_result_t result  = parse_autolink(text, pos, output);
      if (result == MARKER_OK)
        continue;
      *pos = old_pos;
    }

    /* Handle inline HTML */
    if (parser->config.enable_inline_html && ch == '<') {
      /* Simple HTML tag detection */
      size_t tag_end = *pos + 1;
      while (text[tag_end] && text[tag_end] != '>') {
        tag_end++;
      }

      if (text[tag_end] == '>') {
        /* Pass through HTML tag */
        marker_result_t result = buffer_append(output, text + *pos, tag_end - *pos + 1);
        if (result != MARKER_OK)
          return result;
        *pos = tag_end + 1;
        continue;
      }
    }

    /* Handle line breaks */
    if (ch == '\n') {
      if (parser->config.hard_line_breaks) {
        marker_result_t result = buffer_append_str(output, "<br>");
        if (result != MARKER_OK)
          return result;
      } else {
        marker_result_t result = buffer_append_char(output, ' ');
        if (result != MARKER_OK)
          return result;
      }
      (*pos)++;
      continue;
    }

    /* Regular character */
    if (parser->config.escape_html) {
      marker_result_t result = append_escaped_html(output, &ch, 1);
      if (result != MARKER_OK)
        return result;
    } else {
      marker_result_t result = buffer_append_char(output, ch);
      if (result != MARKER_OK)
        return result;
    }

    (*pos)++;
  }

  return MARKER_OK;
}

/* Block parsing functions */
static bool is_header_line(const char* line) { return line[0] == '#'; }

static bool is_code_fence(const char* line) {
  return starts_with(line, "```") || starts_with(line, "~~~");
}

static bool is_blockquote(const char* line) { return line[0] == '>'; }

static bool is_list_item(const char* line) {
  if ((line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ') {
    return true;
  }

  /* Check for ordered list */
  size_t i = 0;
  while (isdigit((unsigned char) line[i]))
    i++;
  return i > 0 && line[i] == '.' && line[i + 1] == ' ';
}

static bool is_horizontal_rule(const char* line) {
  size_t count  = 0;
  char   marker = 0;

  for (size_t i = 0; line[i]; i++) {
    if (line[i] == '-' || line[i] == '*' || line[i] == '_') {
      if (marker == 0) {
        marker = line[i];
      } else if (marker != line[i]) {
        return false;
      }
      count++;
    } else if (!is_whitespace(line[i])) {
      return false;
    }
  }

  return count >= 3;
}

static bool is_table_separator(const char* line) {
  bool has_pipe = false;
  bool has_dash = false;

  for (size_t i = 0; line[i]; i++) {
    if (line[i] == '|') {
      has_pipe = true;
    } else if (line[i] == '-') {
      has_dash = true;
    } else if (line[i] != ' ' && line[i] != ':') {
      return false;
    }
  }

  return has_pipe && has_dash;
}

static marker_result_t parse_header(marker_parser_t* parser, const char* line,
                                    marker_buffer_t* output) {
  if (!is_header_line(line))
    return MARKER_ERROR_INVALID_INPUT;

  int level = 0;
  while (line[level] == '#' && level < 6) {
    level++;
  }

  /* Skip whitespace after # marks */
  size_t content_start = level;
  while (line[content_start] == ' ') {
    content_start++;
  }

  char tag[8];
  snprintf(tag, sizeof(tag), "h%d", level);

  marker_result_t result = buffer_append_str(output, "<");
  if (result != MARKER_OK)
    return result;
  result = buffer_append_str(output, tag);
  if (result != MARKER_OK)
    return result;
  result = buffer_append_str(output, ">");
  if (result != MARKER_OK)
    return result;

  /* Parse inline content */
  size_t pos      = content_start;
  size_t line_len = strlen(line);
  result          = parse_inline_content(parser, line, &pos, output, line_len);
  if (result != MARKER_OK)
    return result;

  result = buffer_append_str(output, "</");
  if (result != MARKER_OK)
    return result;
  result = buffer_append_str(output, tag);
  if (result != MARKER_OK)
    return result;
  result = buffer_append_str(output, ">\n");
  if (result != MARKER_OK)
    return result;

  return MARKER_OK;
}

static marker_result_t parse_blockquote(marker_parser_t* parser, const char* line,
                                        marker_buffer_t* output) {
  if (!is_blockquote(line))
    return MARKER_ERROR_INVALID_INPUT;

  size_t content_start = 1;
  if (line[content_start] == ' ') {
    content_start++;
  }

  marker_result_t result = buffer_append_str(output, "<blockquote>");
  if (result != MARKER_OK)
    return result;

  /* Parse inline content */
  size_t pos      = content_start;
  size_t line_len = strlen(line);
  result          = parse_inline_content(parser, line, &pos, output, line_len);
  if (result != MARKER_OK)
    return result;

  result = buffer_append_str(output, "</blockquote>\n");
  if (result != MARKER_OK)
    return result;

  return MARKER_OK;
}

static marker_result_t parse_list_item(marker_parser_t* parser, const char* line,
                                       marker_buffer_t* output, bool* is_ordered) {
  if (!is_list_item(line))
    return MARKER_ERROR_INVALID_INPUT;

  size_t content_start = 0;
  *is_ordered          = false;

  if (isdigit((unsigned char) line[0])) {
    *is_ordered = true;
    while (isdigit((unsigned char) line[content_start])) {
      content_start++;
    }
    content_start += 2; /* Skip ". " */
  } else {
    content_start = 2; /* Skip "- " or "* " */
  }

  /* Check for task list */
  bool is_task    = false;
  bool is_checked = false;

  if (parser->config.enable_task_lists && line[content_start] == '[' &&
      (line[content_start + 1] == ' ' || line[content_start + 1] == 'x' ||
       line[content_start + 1] == 'X') &&
      line[content_start + 2] == ']' && line[content_start + 3] == ' ') {
    is_task    = true;
    is_checked = (line[content_start + 1] == 'x' || line[content_start + 1] == 'X');
    content_start += 4;
  }

  marker_result_t result = buffer_append_str(output, "<li");
  if (result != MARKER_OK)
    return result;

  if (is_task) {
    result = buffer_append_str(output, " class=\"task-list-item\"");
    if (result != MARKER_OK)
      return result;
  }

  result = buffer_append_str(output, ">");
  if (result != MARKER_OK)
    return result;

  if (is_task) {
    result = buffer_append_str(output, "<input type=\"checkbox\"");
    if (result != MARKER_OK)
      return result;
    if (is_checked) {
      result = buffer_append_str(output, " checked");
      if (result != MARKER_OK)
        return result;
    }
    result = buffer_append_str(output, " disabled> ");
    if (result != MARKER_OK)
      return result;
  }

  /* Parse inline content */
  size_t pos      = content_start;
  size_t line_len = strlen(line);
  result          = parse_inline_content(parser, line, &pos, output, line_len);
  if (result != MARKER_OK)
    return result;

  result = buffer_append_str(output, "</li>\n");
  if (result != MARKER_OK)
    return result;

  return MARKER_OK;
}

static marker_result_t parse_table_row(marker_parser_t* parser, const char* line,
                                       marker_buffer_t* output, bool is_header) {
  const char* tag = is_header ? "th" : "td";

  marker_result_t result = buffer_append_str(output, "<tr>");
  if (result != MARKER_OK)
    return result;

  size_t pos      = 0;
  size_t line_len = strlen(line);

  /* Skip leading whitespace and pipe */
  while (pos < line_len && (is_whitespace(line[pos]) || line[pos] == '|')) {
    pos++;
  }

  while (pos < line_len) {
    result = buffer_append_str(output, "<");
    if (result != MARKER_OK)
      return result;
    result = buffer_append_str(output, tag);
    if (result != MARKER_OK)
      return result;
    result = buffer_append_str(output, ">");
    if (result != MARKER_OK)
      return result;

    /* Find end of cell */
    size_t cell_start = pos;
    while (pos < line_len && line[pos] != '|') {
      pos++;
    }

    size_t cell_end = pos;

    /* Trim whitespace from cell content */
    while (cell_end > cell_start && is_whitespace(line[cell_end - 1])) {
      cell_end--;
    }
    while (cell_start < cell_end && is_whitespace(line[cell_start])) {
      cell_start++;
    }

    /* Parse cell content */
    if (cell_end > cell_start) {
      char* cell_content = malloc(cell_end - cell_start + 1);
      if (!cell_content)
        return MARKER_ERROR_MEMORY_ALLOCATION;

      memcpy(cell_content, line + cell_start, cell_end - cell_start);
      cell_content[cell_end - cell_start] = '\0';

      size_t cell_pos = 0;
      result = parse_inline_content(parser, cell_content, &cell_pos, output, cell_end - cell_start);
      free(cell_content);
      if (result != MARKER_OK)
        return result;
    }

    result = buffer_append_str(output, "</");
    if (result != MARKER_OK)
      return result;
    result = buffer_append_str(output, tag);
    if (result != MARKER_OK)
      return result;
    result = buffer_append_str(output, ">");
    if (result != MARKER_OK)
      return result;

    /* Skip pipe and whitespace */
    if (pos < line_len && line[pos] == '|') {
      pos++;
    }
    while (pos < line_len && is_whitespace(line[pos])) {
      pos++;
    }
  }

  result = buffer_append_str(output, "</tr>\n");
  if (result != MARKER_OK)
    return result;

  return MARKER_OK;
}

static marker_result_t parse_paragraph(marker_parser_t* parser, const char* line,
                                       marker_buffer_t* output) {
  marker_result_t result = buffer_append_str(output, "<p>");
  if (result != MARKER_OK)
    return result;

  size_t pos      = 0;
  size_t line_len = strlen(line);
  result          = parse_inline_content(parser, line, &pos, output, line_len);
  if (result != MARKER_OK)
    return result;

  result = buffer_append_str(output, "</p>\n");
  if (result != MARKER_OK)
    return result;

  return MARKER_OK;
}

/* Main parsing function */
marker_result_t marker_parse(marker_parser_t* parser, const char* markdown,
                             marker_buffer_t* output) {
  if (!parser || !markdown || !output)
    return MARKER_ERROR_NULL_POINTER;

  const char* p               = markdown;
  bool        in_code_block   = false;
  bool        in_list         = false;
  bool        list_is_ordered = false;
  bool        in_table        = false;

  while (*p) {
    /* Extract line */
    size_t line_len = 0;
    while (p[line_len] && p[line_len] != '\n') {
      line_len++;
    }

    if (line_len >= parser->line_buffer_size) {
      char* new_buffer = realloc(parser->line_buffer, line_len + 1);
      if (!new_buffer)
        return MARKER_ERROR_MEMORY_ALLOCATION;
      parser->line_buffer      = new_buffer;
      parser->line_buffer_size = line_len + 1;
    }

    memcpy(parser->line_buffer, p, line_len);
    parser->line_buffer[line_len] = '\0';

    char* line = parser->line_buffer;
    trim_whitespace(line);

    /* Handle code blocks */
    if (is_code_fence(line)) {
      if (!in_code_block) {
        marker_result_t result = buffer_append_str(output, "<pre><code>");
        if (result != MARKER_OK)
          return result;
        in_code_block = true;
      } else {
        marker_result_t result = buffer_append_str(output, "</code></pre>\n");
        if (result != MARKER_OK)
          return result;
        in_code_block = false;
      }
    } else if (in_code_block) {
      /* Inside code block - output verbatim with HTML escaping */
      marker_result_t result = append_escaped_html(output, line, strlen(line));
      if (result != MARKER_OK)
        return result;
      result = buffer_append_char(output, '\n');
      if (result != MARKER_OK)
        return result;
    } else {
      /* Check for reference link definitions */
      if (line[0] == '[') {
        char* closing = strchr(line, ']');
        if (closing && closing[1] == ':') {
          /* Parse reference link definition */
          size_t label_len = closing - line - 1;
          char*  label     = malloc(label_len + 1);
          if (label) {
            memcpy(label, line + 1, label_len);
            label[label_len] = '\0';

            char* url_start = closing + 2;
            while (*url_start == ' ' || *url_start == '\t')
              url_start++;

            char* url_end = url_start;
            while (*url_end && !is_whitespace(*url_end)) {
              url_end++;
            }

            size_t url_len = url_end - url_start;
            char*  url     = malloc(url_len + 1);
            if (url) {
              memcpy(url, url_start, url_len);
              url[url_len] = '\0';

              char* title_start = url_end;
              while (*title_start == ' ' || *title_start == '\t')
                title_start++;

              char* title = NULL;
              if (*title_start == '"') {
                title_start++;
                char* title_end = strrchr(title_start, '"');
                if (title_end) {
                  size_t title_len = title_end - title_start;
                  title            = malloc(title_len + 1);
                  if (title) {
                    memcpy(title, title_start, title_len);
                    title[title_len] = '\0';
                  }
                }
              }

              marker_add_reference_link(parser, label, url, title);
              free(url);
              if (title)
                free(title);
            }
            free(label);
          }

          /* Skip to next line */
          p += line_len;
          if (*p == '\n')
            p++;
          continue;
        }
      }

      /* Handle empty lines */
      if (strlen(line) == 0) {
        if (in_list) {
          marker_result_t result =
              buffer_append_str(output, list_is_ordered ? "</ol>\n" : "</ul>\n");
          if (result != MARKER_OK)
            return result;
          in_list = false;
        }
        if (in_table) {
          marker_result_t result = buffer_append_str(output, "</tbody></table>\n");
          if (result != MARKER_OK)
            return result;
          in_table = false;
        }
        marker_result_t result = buffer_append_char(output, '\n');
        if (result != MARKER_OK)
          return result;
      }
      /* Handle headers */
      else if (is_header_line(line)) {
        if (in_list) {
          marker_result_t result =
              buffer_append_str(output, list_is_ordered ? "</ol>\n" : "</ul>\n");
          if (result != MARKER_OK)
            return result;
          in_list = false;
        }
        if (in_table) {
          marker_result_t result = buffer_append_str(output, "</tbody></table>\n");
          if (result != MARKER_OK)
            return result;
          in_table = false;
        }
        marker_result_t result = parse_header(parser, line, output);
        if (result != MARKER_OK)
          return result;
      }
      /* Handle horizontal rules */
      else if (is_horizontal_rule(line)) {
        if (in_list) {
          marker_result_t result =
              buffer_append_str(output, list_is_ordered ? "</ol>\n" : "</ul>\n");
          if (result != MARKER_OK)
            return result;
          in_list = false;
        }
        if (in_table) {
          marker_result_t result = buffer_append_str(output, "</tbody></table>\n");
          if (result != MARKER_OK)
            return result;
          in_table = false;
        }
        marker_result_t result = buffer_append_str(output, "<hr>\n");
        if (result != MARKER_OK)
          return result;
      }
      /* Handle blockquotes */
      else if (is_blockquote(line)) {
        if (in_list) {
          marker_result_t result =
              buffer_append_str(output, list_is_ordered ? "</ol>\n" : "</ul>\n");
          if (result != MARKER_OK)
            return result;
          in_list = false;
        }
        if (in_table) {
          marker_result_t result = buffer_append_str(output, "</tbody></table>\n");
          if (result != MARKER_OK)
            return result;
          in_table = false;
        }
        marker_result_t result = parse_blockquote(parser, line, output);
        if (result != MARKER_OK)
          return result;
      }
      /* Handle list items */
      else if (is_list_item(line)) {
        bool item_is_ordered;

        if (in_table) {
          marker_result_t result = buffer_append_str(output, "</tbody></table>\n");
          if (result != MARKER_OK)
            return result;
          in_table = false;
        }

        /* Check if we need to start a new list */
        if (!in_list) {
          /* Determine list type from first item */
          bool dummy;
          parse_list_item(parser, line, NULL, &dummy); /* Just to get the type */
          list_is_ordered = isdigit((unsigned char) line[0]);

          marker_result_t result = buffer_append_str(output, list_is_ordered ? "<ol>\n" : "<ul>\n");
          if (result != MARKER_OK)
            return result;
          in_list = true;
        }

        marker_result_t result = parse_list_item(parser, line, output, &item_is_ordered);
        if (result != MARKER_OK)
          return result;
      }
      /* Handle tables */
      else if (parser->config.enable_tables && strchr(line, '|')) {
        if (in_list) {
          marker_result_t result =
              buffer_append_str(output, list_is_ordered ? "</ol>\n" : "</ul>\n");
          if (result != MARKER_OK)
            return result;
          in_list = false;
        }

        /* Check if next line is table separator */
        const char* next_line_start = p + line_len;
        if (*next_line_start == '\n')
          next_line_start++;

        size_t next_line_len = 0;
        while (next_line_start[next_line_len] && next_line_start[next_line_len] != '\n') {
          next_line_len++;
        }

        char* next_line = malloc(next_line_len + 1);
        if (next_line) {
          memcpy(next_line, next_line_start, next_line_len);
          next_line[next_line_len] = '\0';
          trim_whitespace(next_line);

          if (is_table_separator(next_line)) {
            /* Start table */
            if (!in_table) {
              marker_result_t result = buffer_append_str(output, "<table>\n<thead>\n");
              if (result != MARKER_OK) {
                free(next_line);
                return result;
              }
            }

            /* Parse header row */
            marker_result_t result = parse_table_row(parser, line, output, true);
            if (result != MARKER_OK) {
              free(next_line);
              return result;
            }

            result = buffer_append_str(output, "</thead>\n<tbody>\n");
            if (result != MARKER_OK) {
              free(next_line);
              return result;
            }

            in_table = true;

            /* Skip separator line */
            p = next_line_start + next_line_len;
            if (*p == '\n')
              p++;

            free(next_line);
            continue;
          }
          free(next_line);
        }

        if (in_table) {
          /* Parse table data row */
          marker_result_t result = parse_table_row(parser, line, output, false);
          if (result != MARKER_OK)
            return result;
        } else {
          /* Regular paragraph */
          marker_result_t result = parse_paragraph(parser, line, output);
          if (result != MARKER_OK)
            return result;
        }
      }
      /* Handle regular paragraphs */
      else {
        if (in_list) {
          marker_result_t result =
              buffer_append_str(output, list_is_ordered ? "</ol>\n" : "</ul>\n");
          if (result != MARKER_OK)
            return result;
          in_list = false;
        }
        if (in_table) {
          marker_result_t result = buffer_append_str(output, "</tbody></table>\n");
          if (result != MARKER_OK)
            return result;
          in_table = false;
        }
        marker_result_t result = parse_paragraph(parser, line, output);
        if (result != MARKER_OK)
          return result;
      }
    }

    /* Move to next line */
    p += line_len;
    if (*p == '\n')
      p++;
  }

  /* Close any remaining open elements */
  if (in_code_block) {
    marker_result_t result = buffer_append_str(output, "</code></pre>\n");
    if (result != MARKER_OK)
      return result;
  }
  if (in_list) {
    marker_result_t result = buffer_append_str(output, list_is_ordered ? "</ol>\n" : "</ul>\n");
    if (result != MARKER_OK)
      return result;
  }
  if (in_table) {
    marker_result_t result = buffer_append_str(output, "</tbody></table>\n");
    if (result != MARKER_OK)
      return result;
  }

  return MARKER_OK;
}

/* Simplified API functions */
marker_result_t marker_to_html(const char* markdown, char* html, size_t html_size,
                               const char* css_file) {
  if (!markdown || !html || html_size == 0)
    return MARKER_ERROR_NULL_POINTER;

  marker_parser_t* parser = marker_parser_new(NULL);
  if (!parser)
    return MARKER_ERROR_MEMORY_ALLOCATION;

  marker_buffer_t* buffer = marker_buffer_new(html_size);
  if (!buffer) {
    marker_parser_free(parser);
    return MARKER_ERROR_MEMORY_ALLOCATION;
  }

  /* Add HTML document structure */
  marker_result_t result = buffer_append_str(buffer, "<!DOCTYPE html><html><head>");
  if (result != MARKER_OK) {
    marker_buffer_free(buffer);
    marker_parser_free(parser);
    return result;
  }

  if (css_file && strlen(css_file) > 0) {
    result = buffer_append_str(buffer, "<link rel=\"stylesheet\" href=\"");
    if (result != MARKER_OK) {
      marker_buffer_free(buffer);
      marker_parser_free(parser);
      return result;
    }
    result = buffer_append_str(buffer, css_file);
    if (result != MARKER_OK) {
      marker_buffer_free(buffer);
      marker_parser_free(parser);
      return result;
    }
    result = buffer_append_str(buffer, "\">");
    if (result != MARKER_OK) {
      marker_buffer_free(buffer);
      marker_parser_free(parser);
      return result;
    }
  }

  result = buffer_append_str(buffer, "</head><body>");
  if (result != MARKER_OK) {
    marker_buffer_free(buffer);
    marker_parser_free(parser);
    return result;
  }

  /* Parse markdown content */
  result = marker_parse(parser, markdown, buffer);
  if (result != MARKER_OK) {
    marker_buffer_free(buffer);
    marker_parser_free(parser);
    return result;
  }

  result = buffer_append_str(buffer, "</body></html>");
  if (result != MARKER_OK) {
    marker_buffer_free(buffer);
    marker_parser_free(parser);
    return result;
  }

  /* Copy to output buffer */
  size_t output_size = marker_buffer_size(buffer);
  if (output_size >= html_size) {
    marker_buffer_free(buffer);
    marker_parser_free(parser);
    return MARKER_ERROR_BUFFER_TOO_SMALL;
  }

  strcpy(html, marker_buffer_data(buffer));

  marker_buffer_free(buffer);
  marker_parser_free(parser);
  return MARKER_OK;
}

marker_result_t marker_parse_inline(marker_parser_t* parser, const char* text,
                                    marker_buffer_t* output) {
  if (!parser || !text || !output)
    return MARKER_ERROR_NULL_POINTER;

  size_t pos = 0;
  return parse_inline_content(parser, text, &pos, output, strlen(text));
}

marker_result_t marker_file_to_html_file(const char* input_filename, const char* output_filename,
                                         const char* css_file) {
  if (!input_filename || !output_filename)
    return MARKER_ERROR_NULL_POINTER;

  FILE* fin = fopen(input_filename, "r");
  if (!fin)
    return MARKER_ERROR_IO_FAILED;

  fseek(fin, 0, SEEK_END);
  long filesize = ftell(fin);
  if (filesize < 0) {
    fclose(fin);
    return MARKER_ERROR_IO_FAILED;
  }

  fseek(fin, 0, SEEK_SET);
  char* markdown = malloc(filesize + 1);
  if (!markdown) {
    fclose(fin);
    return MARKER_ERROR_MEMORY_ALLOCATION;
  }

  size_t bytes_read    = fread(markdown, 1, filesize, fin);
  markdown[bytes_read] = '\0';
  fclose(fin);

  marker_parser_t* parser = marker_parser_new(NULL);
  if (!parser) {
    free(markdown);
    return MARKER_ERROR_MEMORY_ALLOCATION;
  }

  marker_buffer_t* buffer = marker_buffer_new(0);
  if (!buffer) {
    free(markdown);
    marker_parser_free(parser);
    return MARKER_ERROR_MEMORY_ALLOCATION;
  }

  /* Add HTML document structure */
  marker_result_t result = buffer_append_str(buffer, "<!DOCTYPE html><html><head>");
  if (result == MARKER_OK && css_file && strlen(css_file) > 0) {
    result = buffer_append_str(buffer, "<link rel=\"stylesheet\" href=\"");
    if (result == MARKER_OK)
      result = buffer_append_str(buffer, css_file);
    if (result == MARKER_OK)
      result = buffer_append_str(buffer, "\">");
  }
  if (result == MARKER_OK)
    result = buffer_append_str(buffer, "</head><body>");
  if (result == MARKER_OK)
    result = marker_parse(parser, markdown, buffer);
  if (result == MARKER_OK)
    result = buffer_append_str(buffer, "</body></html>");

  if (result != MARKER_OK) {
    free(markdown);
    marker_buffer_free(buffer);
    marker_parser_free(parser);
    return result;
  }

  FILE* fout = fopen(output_filename, "w");
  if (!fout) {
    free(markdown);
    marker_buffer_free(buffer);
    marker_parser_free(parser);
    return MARKER_ERROR_IO_FAILED;
  }

  fputs(marker_buffer_data(buffer), fout);
  fclose(fout);

  free(markdown);
  marker_buffer_free(buffer);
  marker_parser_free(parser);
  return MARKER_OK;
}

marker_result_t marker_files_to_html_files(const char** input_files, const char** output_files,
                                           int count, const char* css_file) {
  if (!input_files || !output_files || count <= 0)
    return MARKER_ERROR_NULL_POINTER;

  for (int i = 0; i < count; i++) {
    marker_result_t result = marker_file_to_html_file(input_files[i], output_files[i], css_file);
    if (result != MARKER_OK)
      return result;
  }
  return MARKER_OK;
}

bool marker_validate(const char* markdown, char* error_msg, size_t error_msg_size) {
  if (!markdown) {
    if (error_msg && error_msg_size > 0) {
      strncpy(error_msg, "Null markdown input", error_msg_size - 1);
      error_msg[error_msg_size - 1] = '\0';
    }
    return false;
  }

  /* Basic validation - check for common syntax errors */
  const char* p                = markdown;
  int         code_fence_count = 0;

  while (*p) {
    if (strncmp(p, "```", 3) == 0) {
      code_fence_count++;
      p += 3;
      continue;
    }
    p++;
  }

  if (code_fence_count % 2 != 0) {
    if (error_msg && error_msg_size > 0) {
      strncpy(error_msg, "Unclosed code fence", error_msg_size - 1);
      error_msg[error_msg_size - 1] = '\0';
    }
    return false;
  }

  return true;
}

/* Legacy API compatibility */
void md_to_html(const char* markdown, char* html, size_t html_size, const char* css_file) {
  marker_to_html(markdown, html, html_size, css_file);
}

int md_file_to_html_file(const char* input_filename, const char* output_filename,
                         const char* css_file) {
  marker_result_t result = marker_file_to_html_file(input_filename, output_filename, css_file);
  return (result == MARKER_OK) ? 0 : (int) result;
}

int md_files_to_html_files(const char** input_files, const char** output_files, int count,
                           const char* css_file) {
  marker_result_t result = marker_files_to_html_files(input_files, output_files, count, css_file);
  return (result == MARKER_OK) ? 0 : (int) result;
}