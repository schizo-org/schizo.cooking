#include "../src/marker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_BUFFER_SIZE 16384
#define ASSERT_HTML_CONTAINS(html, expected)                                                       \
  do {                                                                                             \
    if (!strstr(html, expected)) {                                                                 \
      fprintf(stderr, "FAIL: Expected '%s' in HTML output\n", expected);                           \
      fprintf(stderr, "Got: %s\n", html);                                                          \
      assert(0);                                                                                   \
    }                                                                                              \
  } while (0)

#define ASSERT_HTML_NOT_CONTAINS(html, unexpected)                                                 \
  do {                                                                                             \
    if (strstr(html, unexpected)) {                                                                \
      fprintf(stderr, "FAIL: Did not expect '%s' in HTML output\n", unexpected);                   \
      fprintf(stderr, "Got: %s\n", html);                                                          \
      assert(0);                                                                                   \
    }                                                                                              \
  } while (0)

// Basic test case. Establish a barebones document with common Markdown features
// and try to parse it. If this fails, then what am I even doing?
static void test_basic_formatting(void) {
  printf("Testing basic formatting...\n");

  marker_parser_t* parser = marker_parser_new(NULL);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "# Header 1\n"
                         "## Header 2\n"
                         "### Header 3\n"
                         "\n"
                         "This is **bold** and this is *italic*.\n"
                         "This is __also bold__ and this is _also italic_.\n"
                         "\n"
                         "Here is `inline code`.\n"
                         "\n"
                         "```\n"
                         "function hello() {\n"
                         "    return \"world\";\n"
                         "}\n"
                         "```\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  ASSERT_HTML_CONTAINS(html, "<h1>Header 1</h1>");
  ASSERT_HTML_CONTAINS(html, "<h2>Header 2</h2>");
  ASSERT_HTML_CONTAINS(html, "<h3>Header 3</h3>");
  ASSERT_HTML_CONTAINS(html, "<strong>bold</strong>");
  ASSERT_HTML_CONTAINS(html, "<em>italic</em>");
  ASSERT_HTML_CONTAINS(html, "<strong>also bold</strong>");
  ASSERT_HTML_CONTAINS(html, "<em>also italic</em>");
  ASSERT_HTML_CONTAINS(html, "<code>inline code</code>");
  ASSERT_HTML_CONTAINS(html, "<pre><code>");
  ASSERT_HTML_CONTAINS(html, "function hello()");
  ASSERT_HTML_CONTAINS(html, "</code></pre>");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test links and images. This is not the most common usecase, but surely we will
// need them for schizo.cooking. It's a cooking website after all.
static void test_links_and_images(void) {
  printf("Testing links and images...\n");

  marker_parser_t* parser = marker_parser_new(NULL);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "This is a [link](https://example.com).\n"
                         "This is a [link with title](https://example.com \"Example Site\").\n"
                         "![Alt text](https://example.com/image.png)\n"
                         "![Alt with title](https://example.com/image.png \"Image Title\")\n"
                         "\n"
                         "[ref1]: https://example.com \"Reference Title\"\n"
                         "[ref2]: https://example2.com\n"
                         "\n"
                         "[Reference link][ref1]\n"
                         "[Another ref][ref2]\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  ASSERT_HTML_CONTAINS(html, "<a href=\"https://example.com\">link</a>");
  ASSERT_HTML_CONTAINS(html, "title=\"Example Site\"");
  ASSERT_HTML_CONTAINS(html, "<img src=\"https://example.com/image.png\" alt=\"Alt text\">");
  ASSERT_HTML_CONTAINS(html, "title=\"Image Title\"");
  ASSERT_HTML_CONTAINS(
      html, "<a href=\"https://example.com\" title=\"Reference Title\">Reference link</a>");
  ASSERT_HTML_CONTAINS(html, "<a href=\"https://example2.com\">Another ref</a>");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test lists. This previously bit me in the ass, because Markdown hates me.
static void test_lists(void) {
  printf("Testing lists...\n");

  marker_parser_t* parser = marker_parser_new(NULL);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "Unordered list:\n"
                         "- Item 1\n"
                         "- Item 2\n"
                         "* Item 3\n"
                         "+ Item 4\n"
                         "\n"
                         "Ordered list:\n"
                         "1. First item\n"
                         "2. Second item\n"
                         "3. Third item\n"
                         "\n"
                         "Task list:\n"
                         "- [x] Completed task\n"
                         "- [ ] Incomplete task\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  ASSERT_HTML_CONTAINS(html, "<ul>");
  ASSERT_HTML_CONTAINS(html, "</ul>");
  ASSERT_HTML_CONTAINS(html, "<ol>");
  ASSERT_HTML_CONTAINS(html, "</ol>");
  ASSERT_HTML_CONTAINS(html, "<li>Item 1</li>");
  ASSERT_HTML_CONTAINS(html, "<li>First item</li>");
  ASSERT_HTML_CONTAINS(html, "class=\"task-list-item\"");
  ASSERT_HTML_CONTAINS(html, "type=\"checkbox\" checked disabled");
  ASSERT_HTML_CONTAINS(html, "type=\"checkbox\" disabled");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test blockquotes and horizontal rules
static void test_blockquotes_and_rules(void) {
  printf("Testing blockquotes and horizontal rules...\n");

  marker_parser_t* parser = marker_parser_new(NULL);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "> This is a blockquote.\n"
                         "> It can span multiple lines.\n"
                         "\n"
                         "---\n"
                         "\n"
                         "*** \n"
                         "\n"
                         "___\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  ASSERT_HTML_CONTAINS(html, "<blockquote>");
  ASSERT_HTML_CONTAINS(html, "</blockquote>");
  ASSERT_HTML_CONTAINS(html, "<hr>");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// As documented, Marker supports some of GFM extensions.
// Test GFM extensions.
static void test_gfm_features(void) {
  printf("Testing GFM features...\n");

  marker_parser_t* parser = marker_parser_new(NULL);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "This is ~~strikethrough~~ text.\n"
                         "\n"
                         "| Header 1 | Header 2 | Header 3 |\n"
                         "|----------|----------|----------|\n"
                         "| Cell 1   | Cell 2   | Cell 3   |\n"
                         "| Cell 4   | Cell 5   | Cell 6   |\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  ASSERT_HTML_CONTAINS(html, "<del>strikethrough</del>");
  ASSERT_HTML_CONTAINS(html, "<table>");
  ASSERT_HTML_CONTAINS(html, "<thead>");
  ASSERT_HTML_CONTAINS(html, "<tbody>");
  ASSERT_HTML_CONTAINS(html, "<th>Header 1</th>");
  ASSERT_HTML_CONTAINS(html, "<td>Cell 1</td>");
  ASSERT_HTML_CONTAINS(html, "</table>");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test autolinks
static void test_autolinks(void) {
  printf("Testing autolinks...\n");

  marker_parser_t* parser = marker_parser_new(NULL);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "Visit <https://example.com> for more info.\n"
                         "Email me at <test@example.com>.\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  ASSERT_HTML_CONTAINS(html, "<a href=\"https://example.com\">https://example.com</a>");
  ASSERT_HTML_CONTAINS(html, "<a href=\"mailto:test@example.com\">test@example.com</a>");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test HTML escaping
static void test_html_escaping(void) {
  printf("Testing HTML escaping...\n");

  marker_config_t config;
  marker_config_init(&config);
  config.escape_html        = true;
  config.enable_inline_html = false;

  marker_parser_t* parser = marker_parser_new(&config);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "This contains <script> tags & \"quotes\".\n"
                         "`Code with <html> & entities`\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  ASSERT_HTML_CONTAINS(html, "&lt;script&gt;");
  ASSERT_HTML_CONTAINS(html, "&amp;");
  ASSERT_HTML_CONTAINS(html, "&quot;");
  ASSERT_HTML_CONTAINS(html, "<code>");
  ASSERT_HTML_CONTAINS(html, "&lt;html&gt;");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test inline HTML
static void test_inline_html(void) {
  printf("Testing inline HTML...\n");

  marker_config_t config;
  marker_config_init(&config);
  config.enable_inline_html = true;
  config.escape_html        = false;

  marker_parser_t* parser = marker_parser_new(&config);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "This has <em>inline HTML</em> tags.\n"
                         "And a <span class=\"highlight\">span</span>.\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  ASSERT_HTML_CONTAINS(html, "<em>inline HTML</em>");
  ASSERT_HTML_CONTAINS(html, "<span class=\"highlight\">span</span>");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Edge cases
static void test_edge_cases(void) {
  printf("Testing edge cases...\n");

  marker_parser_t* parser = marker_parser_new(NULL);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  // Input is empty
  marker_result_t result = marker_parse(parser, "", buffer);
  assert(result == MARKER_OK);

  // Unclosed emphasis, e.g., *foo
  buffer = marker_buffer_new(0);
  result = marker_parse(parser, "This is *unclosed emphasis", buffer);
  assert(result == MARKER_OK);
  const char* html = marker_buffer_data(buffer);
  ASSERT_HTML_CONTAINS(html, "*unclosed emphasis");

  // Nested emphasis
  marker_buffer_free(buffer);
  buffer = marker_buffer_new(0);
  result = marker_parse(parser, "This is ***bold and italic***", buffer);
  assert(result == MARKER_OK);
  html = marker_buffer_data(buffer);
  ASSERT_HTML_CONTAINS(html, "<strong>");
  ASSERT_HTML_CONTAINS(html, "<em>");

  // Esc sequences
  marker_buffer_free(buffer);
  buffer = marker_buffer_new(0);
  result = marker_parse(parser, "\\*Not italic\\* and \\`not code\\`", buffer);
  assert(result == MARKER_OK);
  html = marker_buffer_data(buffer);
  ASSERT_HTML_CONTAINS(html, "*Not italic*");
  ASSERT_HTML_CONTAINS(html, "`not code`");
  ASSERT_HTML_NOT_CONTAINS(html, "<em>");
  ASSERT_HTML_NOT_CONTAINS(html, "<code>");

  // Multiple backticks in a codespan
  marker_buffer_free(buffer);
  buffer = marker_buffer_new(0);
  result = marker_parse(parser, "``Code with ` backtick``", buffer);
  assert(result == MARKER_OK);
  html = marker_buffer_data(buffer);
  ASSERT_HTML_CONTAINS(html, "<code>Code with ` backtick</code>");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test error handling
static void test_error_handling(void) {
  printf("Testing error handling...\n");

  // NULL pointer
  assert(marker_parse(NULL, "test", NULL) == MARKER_ERROR_NULL_POINTER);
  assert(marker_to_html(NULL, NULL, 0, NULL) == MARKER_ERROR_NULL_POINTER);

  char            small_buffer[10];
  marker_result_t result =
      marker_to_html("This is a very long markdown text", small_buffer, sizeof(small_buffer), NULL);
  assert(result == MARKER_ERROR_BUFFER_TOO_SMALL);

  // Validation
  char error_msg[256];
  bool valid = marker_validate("```\nUnclosed code block", error_msg, sizeof(error_msg));
  assert(!valid);
  assert(strstr(error_msg, "Unclosed code fence") != NULL);

  valid = marker_validate("# Normal markdown", error_msg, sizeof(error_msg));
  assert(valid);
}

// Test configuration options
static void test_configuration(void) {
  printf("Testing configuration options...\n");

  marker_config_t config;
  marker_config_init(&config);

  // Can we disable features?
  config.enable_tables        = false;
  config.enable_strikethrough = false;
  config.enable_task_lists    = false;

  marker_parser_t* parser = marker_parser_new(&config);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  const char* markdown = "~~strikethrough~~\n"
                         "- [x] task\n"
                         "| table | cell |\n"
                         "|-------|------|\n";

  marker_result_t result = marker_parse(parser, markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);

  /* Should not contain GFM features */
  ASSERT_HTML_NOT_CONTAINS(html, "<del>");
  ASSERT_HTML_NOT_CONTAINS(html, "task-list-item");
  ASSERT_HTML_NOT_CONTAINS(html, "<table>");

  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test legacy API compat
// I don't think anyone used this library but the fact that there is a legacy
// API does mean I will keep using it for a while. Because I'm lazy. Not lazy
// enough to avoid tests though.
static void test_legacy_api(void) {
  printf("Testing legacy API compatibility...\n");

  char        html_output[TEST_BUFFER_SIZE];
  const char* markdown = "# Legacy Test\n"
                         "This tests the old API.\n"
                         "**Bold** and *italic*.\n";

  md_to_html(markdown, html_output, sizeof(html_output), "test.css");

  ASSERT_HTML_CONTAINS(html_output, "<!DOCTYPE html>");
  ASSERT_HTML_CONTAINS(html_output, "<link rel=\"stylesheet\" href=\"test.css\">");
  ASSERT_HTML_CONTAINS(html_output, "<h1>Legacy Test</h1>");
  ASSERT_HTML_CONTAINS(html_output, "<strong>Bold</strong>");
  ASSERT_HTML_CONTAINS(html_output, "<em>italic</em>");
  ASSERT_HTML_CONTAINS(html_output, "</body></html>");
}

// Test document generation
static void test_full_document(void) {
  printf("Testing full document generation...\n");

  char        html_output[TEST_BUFFER_SIZE];
  const char* markdown = "# Document Title\n"
                         "\n"
                         "This is a **complete** document with:\n"
                         "\n"
                         "- Lists\n"
                         "- [Links](https://example.com)\n"
                         "- `Code`\n"
                         "\n"
                         "```javascript\n"
                         "console.log('Hello, world!');\n"
                         "```\n"
                         "\n"
                         "> Blockquotes\n"
                         "\n"
                         "| Tables | Work |\n"
                         "|--------|------|\n"
                         "| Yes    | They do |\n";

  marker_result_t result = marker_to_html(markdown, html_output, sizeof(html_output), "styles.css");
  assert(result == MARKER_OK);

  ASSERT_HTML_CONTAINS(html_output, "<!DOCTYPE html>");
  ASSERT_HTML_CONTAINS(html_output, "<html>");
  ASSERT_HTML_CONTAINS(html_output, "<head>");
  ASSERT_HTML_CONTAINS(html_output, "<link rel=\"stylesheet\" href=\"styles.css\">");
  ASSERT_HTML_CONTAINS(html_output, "</head>");
  ASSERT_HTML_CONTAINS(html_output, "<body>");
  ASSERT_HTML_CONTAINS(html_output, "<h1>Document Title</h1>");
  ASSERT_HTML_CONTAINS(html_output, "</body>");
  ASSERT_HTML_CONTAINS(html_output, "</html>");
}

// Perf test
static void test_performance(void) {
  printf("Testing performance with large input...\n");

  // Try to generate large MD content
  size_t content_size   = 100000;
  char*  large_markdown = malloc(content_size);
  assert(large_markdown != NULL);

  strcpy(large_markdown, "# Large Document\n\n");
  size_t pos = strlen(large_markdown);

  for (int i = 0; i < 100; i++) {
    int remaining = content_size - pos - 200;
    if (remaining <= 0)
      break;

    int written =
        snprintf(large_markdown + pos, remaining,
                 "## Section %d\n\nThis is paragraph %d with **bold** and *italic* text.\n\n"
                 "- List item %d\n"
                 "- Another item\n\n",
                 i, i, i);
    pos += written;
  }

  marker_parser_t* parser = marker_parser_new(NULL);
  assert(parser != NULL);

  marker_buffer_t* buffer = marker_buffer_new(0);
  assert(buffer != NULL);

  marker_result_t result = marker_parse(parser, large_markdown, buffer);
  assert(result == MARKER_OK);

  const char* html = marker_buffer_data(buffer);
  assert(strlen(html) > 0);
  ASSERT_HTML_CONTAINS(html, "<h1>Large Document</h1>");
  ASSERT_HTML_CONTAINS(html, "Section 99");

  free(large_markdown);
  marker_buffer_free(buffer);
  marker_parser_free(parser);
}

// Test utils
static void test_utilities(void) {
  printf("Testing utility functions...\n");

  const char* version = marker_version();
  assert(version != NULL);
  assert(strlen(version) > 0);
  printf("Library version: %s\n", version);

  const char* error_msg = marker_error_string(MARKER_OK);
  assert(strcmp(error_msg, "Success") == 0);

  error_msg = marker_error_string(MARKER_ERROR_NULL_POINTER);
  assert(strstr(error_msg, "Null pointer") != NULL);

  // Test HTML escaping
  char            escaped[256];
  marker_result_t result =
      marker_escape_html("Hello <world> & \"friends\"", escaped, sizeof(escaped));
  assert(result == MARKER_OK);
  assert(strstr(escaped, "&lt;world&gt;") != NULL);
  assert(strstr(escaped, "&amp;") != NULL);
  assert(strstr(escaped, "&quot;friends&quot;") != NULL);
}

int main(void) {
  printf("===Running Marker test suite===\n\n");

  test_basic_formatting();
  test_links_and_images();
  test_lists();
  test_blockquotes_and_rules();
  test_gfm_features();
  test_autolinks();
  test_html_escaping();
  test_inline_html();
  test_edge_cases();
  test_error_handling();
  test_configuration();
  test_legacy_api();
  test_full_document();
  test_performance();
  test_utilities();

  printf("\n===All tests passed===\n");
  return 0;
}
