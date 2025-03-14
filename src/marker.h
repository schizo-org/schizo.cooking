#ifndef MARKER_H
#define MARKER_H

#include <stddef.h>

/**
 * Converts a Markdown string to HTML
 *
 * @param markdown The Markdown input string to convert to HTML
 * @param html The output buffer where the generated HTML will be stored
 * @param html_size The size of the output buffer (html)
 *
 * @note The function does not perform any bounds checking for `markdown` or
 * `html_size`. Ensure that the output buffer is large enough to accommodate the
 * generated HTML or suffer the consequences of your own actions.
 */
void md_to_html(const char *markdown, char *html, size_t html_size);

/**
 * Converts a Markdown file to an HTML file
 *
 *
 * @param input_filename The name of the Markdown input file
 * @param output_filename The name of the output HTML file
 *
 * @return 0 on success, a negative value on failure:
 *         - -1: Failed to open input file
 *         - -2: Memory allocation failure
 *         - -3: Failed to allocate memory for HTML output
 *         - -4: Failed to open output file
 */
int md_file_to_html_file(const char *input_filename,
                         const char *output_filename);

/**
 * Converts multiple Markdown files to HTML files.
 *
 * @param input_files An array of pointers to the Markdown input filenames
 * @param output_files An array of pointers to the output HTML filenames
 * @param count The number of files to process
 *
 * @return 0 on success, a negative value on failure:
 *         - -1: Failure in converting one or more files
 */
int md_files_to_html_files(const char **input_files, const char **output_files,
                           int count);

#endif // MARKER_H
