#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUTPUT_BUFFER_SIZE 8192
#define LINE_BUFFER_SIZE 1024

// Process inline formatting for a given line. Supports:
// - Bold: **text**
// - Italic: *text*
// - Inline code: `code`
// - Links: [text](url)
// - Escape characters: \*, \_, \#, etc.
void inline_parse(const char *in, char *out, size_t out_size) {
  size_t i = 0, j = 0;
  while (in[i] && j < out_size - 1) {
    if (in[i] == '\\') {
      if (in[i + 1]) { // add the escaped character as is
        out[j++] = in[i + 1];
        i += 2;
        continue;
      }
    }

    if (in[i] == '*' && in[i + 1] == '*') {
      const char *openTag = "<b>";
      size_t k = 0;
      while (openTag[k] && j < out_size - 1)
        out[j++] = openTag[k++];
      i += 2;
      while (!(in[i] == '*' && in[i + 1] == '*') && in[i] != '\0') {
        out[j++] = in[i++];
        if (j >= out_size - 1)
          break;
      }
      if (in[i] == '*' && in[i + 1] == '*') {
        const char *closeTag = "</b>";
        k = 0;
        while (closeTag[k] && j < out_size - 1)
          out[j++] = closeTag[k++];
        i += 2;
      }
      continue;
    }

    if (in[i] == '*') {
      const char *openTag = "<i>";
      size_t k = 0;
      while (openTag[k] && j < out_size - 1)
        out[j++] = openTag[k++];
      i++;
      while (in[i] != '*' && in[i] != '\0') {
        out[j++] = in[i++];
        if (j >= out_size - 1)
          break;
      }
      if (in[i] == '*') {
        const char *closeTag = "</i>";
        k = 0;
        while (closeTag[k] && j < out_size - 1)
          out[j++] = closeTag[k++];
        i++;
      }
      continue;
    }

    if (in[i] == '`') {
      const char *openTag = "<code>";
      size_t k = 0;
      while (openTag[k] && j < out_size - 1)
        out[j++] = openTag[k++];
      i++;

      while (in[i] != '`' && in[i] != '\0') {
        out[j++] = in[i++];
        if (j >= out_size - 1)
          break;
      }

      if (in[i] == '`') {
        const char *closeTag = "</code>";
        k = 0;
        while (closeTag[k] && j < out_size - 1)
          out[j++] = closeTag[k++];
        i++;
      }

      continue;
    }

    if (in[i] == '[') {
      size_t link_text_start = i + 1;
      size_t text_len = 0;
      while (in[link_text_start + text_len] &&
             in[link_text_start + text_len] != ']')
        text_len++;
      if (in[link_text_start + text_len] == ']') {
        if (in[link_text_start + text_len + 1] == '(') {
          size_t url_start = link_text_start + text_len + 2;
          size_t url_len = 0;
          while (in[url_start + url_len] && in[url_start + url_len] != ')')
            url_len++;
          if (in[url_start + url_len] == ')') {
            char link_html[LINE_BUFFER_SIZE];
            snprintf(link_html, sizeof(link_html), "<a href=\"%.*s\">%.*s</a>",
                     (int)url_len, in + url_start, (int)text_len,
                     in + link_text_start);
            size_t link_html_len = strlen(link_html);
            if (j + link_html_len < out_size - 1) {
              strcpy(&out[j], link_html);
              j += link_html_len;
            }
            i = url_start + url_len + 1;
            continue;
          }
        }
      }
    }

    out[j++] = in[i++];
  }
  out[j] = '\0';
}

// Process the input markdown text line by line. Supports:
// - Headers (lines beginning with one or more '#' characters)
// - Code blocks (fenced with ```)
// - List items (lines starting with "- " or "* ")
// - Blockquotes (lines starting with '> ')
// - Paragraphs (all other non-empty lines)
void block_parse(const char *input, char *output, size_t out_size) {
  char line[LINE_BUFFER_SIZE];
  size_t output_index = 0;
  int in_code_block = 0;

  const char *p = input;
  while (*p) {
    size_t line_len = 0;
    while (p[line_len] && p[line_len] != '\n' &&
           line_len < LINE_BUFFER_SIZE - 1)
      line[line_len] = p[line_len], line_len++;
    line[line_len] = '\0';
    if (p[line_len] == '\n')
      p += line_len + 1;
    else
      p += line_len;

    if (strncmp(line, "```", 3) == 0) {
      if (!in_code_block) {
        strncat(output, "<pre><code>\n", out_size - strlen(output) - 1);
        in_code_block = 1;
      } else {
        strncat(output, "\n</code></pre>\n", out_size - strlen(output) - 1);
        in_code_block = 0;
      }
      continue;
    }

    if (in_code_block) {
      // We're inside a code block, output the line verbatim
      strncat(output, line, out_size - strlen(output) - 1);
      strncat(output, "\n", out_size - strlen(output) - 1);
      continue;
    }

    if (line[0] == '#') {
      int header_level = 0;
      while (line[header_level] == '#' && header_level < 6)
        header_level++;
      int content_start = header_level;
      while (line[content_start] == ' ')
        content_start++;
      char tag[10];
      sprintf(tag, "h%d", header_level);

      char inline_output[LINE_BUFFER_SIZE] = {0};
      inline_parse(line + content_start, inline_output, sizeof(inline_output));

      char header_html[LINE_BUFFER_SIZE];
      snprintf(header_html, sizeof(header_html),
               "<%s>%s</%s>\n", tag, inline_output, tag);
      strncat(output, header_html, out_size - strlen(output) - 1);
      continue;
    }

    // Blockquote
    if (line[0] == '>') {
      char blockquote_html[LINE_BUFFER_SIZE];
      snprintf(blockquote_html, sizeof(blockquote_html),
               "<blockquote>%s</blockquote>\n", line + 2);
      strncat(output, blockquote_html, out_size - strlen(output) - 1);
      continue;
    }

    if ((line[0] == '-' || line[0] == '*') && line[1] == ' ') {
      char inline_output[LINE_BUFFER_SIZE] = {0};
      inline_parse(line + 2, inline_output, sizeof(inline_output));
      char list_item[LINE_BUFFER_SIZE];
      snprintf(list_item, sizeof(list_item),
               "<li>%s</li>\n", inline_output);
      strncat(output, list_item, out_size - strlen(output) - 1);
      continue;
    }

    if (strlen(line) > 0) {
      char inline_output[LINE_BUFFER_SIZE] = {0};
      inline_parse(line, inline_output, sizeof(inline_output));
      char paragraph[LINE_BUFFER_SIZE];
      snprintf(paragraph, sizeof(paragraph), "<p>%s</p>\n",
               inline_output);
      strncat(output, paragraph, out_size - strlen(output) - 1);
    } else {
      // Blank line.
      strncat(output, "\n", out_size - strlen(output) - 1);
    }
  }
}

// Converts a markdown string into HTML
// Caller must provide an output buffer (html) of at least html_size bytes
// or suffer the consequences
void md_to_html(const char *markdown, char *html, size_t html_size,
                const char *css_file) {
  if (!html || html_size == 0)
    return;

  snprintf(html, html_size, "<!DOCTYPE html><html><head>");
  if (css_file && strlen(css_file) > 0) {
    snprintf(html + strlen(html), html_size - strlen(html),
             "<link rel=\"stylesheet\" href=\"%s\">", css_file);
  }

  snprintf(html + strlen(html), html_size - strlen(html), "</head><body>");

  block_parse(markdown, html + strlen(html), html_size - strlen(html));
  strncat(html, "</body></html>", html_size - strlen(html) - 1);
}

// Convert a markdown file to an HTML file
int md_file_to_html_file(const char *input_filename,
                         const char *output_filename, const char *css_file) {
  FILE *fin = fopen(input_filename, "r");
  if (!fin)
    return -1;

  fseek(fin, 0, SEEK_END);
  long filesize = ftell(fin);

  fseek(fin, 0, SEEK_SET);
  char *markdown = malloc(filesize + 1);
  if (!markdown) {
    fclose(fin);
    return -2;
  }

  fread(markdown, 1, filesize, fin);
  markdown[filesize] = '\0';
  fclose(fin);

  char *html_output = malloc(OUTPUT_BUFFER_SIZE);
  if (!html_output) {
    free(markdown);
    return -3;
  }

  html_output[0] = '\0';
  md_to_html(markdown, html_output, OUTPUT_BUFFER_SIZE, css_file);

  FILE *fout = fopen(output_filename, "w");
  if (!fout) {
    free(markdown);
    free(html_output);
    return -4;
  }

  fputs(html_output, fout);
  fclose(fout);

  free(markdown);
  free(html_output);
  return 0;
}

// Process multiple markdown files
int md_files_to_html_files(const char **input_files, const char **output_files,
                           int count, const char *css_file) {
  for (int i = 0; i < count; i++) {
    if (md_file_to_html_file(input_files[i], output_files[i], css_file) != 0) {
      return -1;
    }
  }
  return 0;
}
