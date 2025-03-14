#include "../src/marker.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define OUTPUT_BUFFER_SIZE 8192

// Low-effort testing implementation. Can be done better, but I don't care.
void test_md_to_html(void) {
  const char *markdown_text =
      "# Hello World\n"
      "This is **bold** and this is *italic*.\n"
      "Here is a link: [Google](https://www.google.com).\n"
      "Inline code: `printf(\"Hello, world!\");`\n"
      "```\n"
      "int main() {\n"
      "   return 0;\n"
      "}\n"
      "```\n"
      "- List item 1\n- List item 2\n"
      "> This is a blockquote.\n";

  char html_output_with_css[OUTPUT_BUFFER_SIZE] = {0};
  char html_output_without_css[OUTPUT_BUFFER_SIZE] = {0};

  const char *css_file = "test.css";
  md_to_html(markdown_text, html_output_with_css, sizeof(html_output_with_css),
             css_file);

  md_to_html(markdown_text, html_output_without_css,
             sizeof(html_output_without_css), NULL);

  // Fuck.
  printf("\nGenerated HTML with CSS:\n---\n%s\n---", html_output_with_css);
  printf("\nGenerated HTML without CSS:\n---\n%s\n---",
         html_output_without_css);

  assert(strstr(html_output_with_css,
                "<link rel=\"stylesheet\" href=\"test.css\">") != NULL);

  assert(strstr(html_output_with_css,
                "<h1 class=\"header-1\">Hello World</h1>") != NULL);
  assert(strstr(html_output_with_css, "<b>bold</b>") != NULL);
  assert(strstr(html_output_with_css, "<i>italic</i>") != NULL);
  assert(strstr(html_output_with_css,
                "<a href=\"https://www.google.com\">Google</a>") != NULL);
  assert(strstr(html_output_with_css,
                "<code>printf(\"Hello, world!\");</code>") != NULL);
  assert(strstr(html_output_with_css, "<pre><code>") !=
         NULL); // code block start
  assert(strstr(html_output_with_css, "</code></pre>") !=
         NULL); // code block end
  assert(strstr(html_output_with_css,
                "<li class=\"list-item\">List item 1</li>") != NULL);
  assert(strstr(html_output_with_css, "<blockquote class=\"blockquote\">This "
                                      "is a blockquote.</blockquote>") != NULL);

  assert(strstr(html_output_without_css,
                "<h1 class=\"header-1\">Hello World</h1>") != NULL);
  assert(strstr(html_output_without_css, "<b>bold</b>") != NULL);
  assert(strstr(html_output_without_css, "<i>italic</i>") != NULL);
  assert(strstr(html_output_without_css,
                "<a href=\"https://www.google.com\">Google</a>") != NULL);
  assert(strstr(html_output_without_css,
                "<code>printf(\"Hello, world!\");</code>") != NULL);
  assert(strstr(html_output_without_css, "<pre><code>") !=
         NULL); // code block start
  assert(strstr(html_output_without_css, "</code></pre>") !=
         NULL); // code block end
  assert(strstr(html_output_without_css,
                "<li class=\"list-item\">List item 1</li>") != NULL);
  assert(strstr(html_output_without_css,
                "<blockquote class=\"blockquote\">This "
                "is a blockquote.</blockquote>") != NULL);

  assert(strstr(html_output_without_css,
                "<link rel=\"stylesheet\" href=\"test.css\">") == NULL);
}

// Best test handling the world has ever seen. Never doubt me.
int main(void) {
  test_md_to_html();
  printf("\nTest passed!\n");
  return 0;
}
