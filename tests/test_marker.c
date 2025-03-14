#include "../src/marker.h"
#include <stdio.h>

#define OUTPUT_BUFFER_SIZE 8192

// Low-effort testing implementation. Can be done better, but I don't care
// to improve it. Caller must manually observe the output. Add some assertions
// if this bothers you, but know that I will dropkick you if you try to use
// regex to parse HTML.
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

  char html_output[OUTPUT_BUFFER_SIZE] = {0};
  md_to_html(markdown_text, html_output, sizeof(html_output));
  printf("%s\n", html_output);
}

int main(void) {
  test_md_to_html();
  return 0;
}
