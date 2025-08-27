#ifndef MARKER_H
#define MARKER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MARKER_VERSION_MAJOR 1
#define MARKER_VERSION_MINOR 0
#define MARKER_VERSION_PATCH 0
#define MARKER_VERSION_STRING "1.0.0"

// Error codes.
// Previously I had elected to omit explicit error handling because Marker
// is a private library, but there is no harm done in supporting custom errors.
typedef enum {
  MARKER_OK                      = 0,
  MARKER_ERROR_NULL_POINTER      = -1,
  MARKER_ERROR_INVALID_SIZE      = -2,
  MARKER_ERROR_BUFFER_TOO_SMALL  = -3,
  MARKER_ERROR_IO_FAILED         = -4,
  MARKER_ERROR_MEMORY_ALLOCATION = -5,
  MARKER_ERROR_INVALID_INPUT     = -6,
  MARKER_ERROR_PARSE_FAILED      = -7
} marker_result_t;

// Configuration options for modifying parser behaviour
typedef struct {
  bool   enable_tables;         // Enable GFM tables
  bool   enable_strikethrough;  // Enable GFM strikethrough (~~text~~)
  bool   enable_task_lists;     // Enable GFM task lists (- [x] item)
  bool   enable_autolinks;      // Enable autolink detection
  bool   enable_inline_html;    // Allow inline HTML passthrough
  bool   escape_html;           // Escape HTML entities in text
  bool   smart_quotes;          // Convert quotes to smart quotes
  bool   hard_line_breaks;      // Treat single line breaks as <br>
  size_t max_nesting_depth;     // Maximum nesting depth for lists/quotes
  size_t initial_buffer_size;   // Initial buffer size for dynamic allocation
} marker_config_t;

// Parser context
typedef struct marker_parser marker_parser_t;

// HTML output buffer
typedef struct {
  char*  data;
  size_t size;
  size_t capacity;
} marker_buffer_t;

// Reference link
typedef struct marker_ref_link {
  char*                   label;
  char*                   url;
  char*                   title;
  struct marker_ref_link* next;
} marker_ref_link_t;

/**
 * Get the library version string
 * @return Version string (e.g., "1.0.0")
 */
const char* marker_version(void);

/**
 * Get error message for a result code
 * @param result Error code
 * @return Human-readable error message
 */
const char* marker_error_string(marker_result_t result);

/**
 * Initialize default configuration
 * @param config Configuration structure to initialize
 */
void marker_config_init(marker_config_t* config);

/**
 * Create a new parser instance with specified configuration
 * @param config Parser configuration (NULL for default)
 * @return Parser instance or NULL on failure
 */
marker_parser_t* marker_parser_new(const marker_config_t* config);

/**
 * Free parser instance and associated resources
 * @param parser Parser instance to free
 */
void marker_parser_free(marker_parser_t* parser);

/**
 * Create a new output buffer
 * @param initial_capacity Initial buffer capacity
 * @return Buffer instance or NULL on failure
 */
marker_buffer_t* marker_buffer_new(size_t initial_capacity);

/**
 * Free output buffer and associated memory
 * @param buffer Buffer to free
 */
void marker_buffer_free(marker_buffer_t* buffer);

/**
 * Get buffer data as null-terminated string
 * @param buffer Buffer instance
 * @return Null-terminated string or NULL if buffer is invalid
 */
const char* marker_buffer_data(const marker_buffer_t* buffer);

/**
 * Get buffer size (excluding null terminator)
 * @param buffer Buffer instance
 * @return Size in bytes
 */
size_t marker_buffer_size(const marker_buffer_t* buffer);

/**
 * Convert Markdown string to HTML using parser instance
 * @param parser Parser instance
 * @param markdown Input Markdown string
 * @param output Output buffer (will be resized as needed)
 * @return Result code
 */
marker_result_t marker_parse(marker_parser_t* parser, const char* markdown,
                             marker_buffer_t* output);

/**
 * Convert Markdown string to HTML with simple API
 * @param markdown Input Markdown string
 * @param html Output buffer for HTML
 * @param html_size Size of output buffer
 * @param css_file Optional CSS file to include (NULL to omit)
 * @return Result code
 */
marker_result_t marker_to_html(const char* markdown, char* html, size_t html_size,
                               const char* css_file);

/**
 * Convert Markdown file to HTML file
 * @param input_filename Input Markdown file
 * @param output_filename Output HTML file
 * @param css_file Optional CSS file to include (NULL to omit)
 * @return Result code
 */
marker_result_t marker_file_to_html_file(const char* input_filename, const char* output_filename,
                                         const char* css_file);

/**
 * Convert multiple Markdown files to HTML files
 * @param input_files Array of input filenames
 * @param output_files Array of output filenames
 * @param count Number of files
 * @param css_file Optional CSS file to include (NULL to omit)
 * @return Result code
 */
marker_result_t marker_files_to_html_files(const char** input_files, const char** output_files,
                                           int count, const char* css_file);

/**
 * Add reference link to parser
 * @param parser Parser instance
 * @param label Reference label
 * @param url URL
 * @param title Optional title (NULL if none)
 * @return Result code
 */
marker_result_t marker_add_reference_link(marker_parser_t* parser, const char* label,
                                          const char* url, const char* title);

/**
 * Clear all reference links from parser
 * @param parser Parser instance
 */
void marker_clear_reference_links(marker_parser_t* parser);

/**
 * Parse only inline elements (no block structure)
 * @param parser Parser instance
 * @param text Input text
 * @param output Output buffer
 * @return Result code
 */
marker_result_t marker_parse_inline(marker_parser_t* parser, const char* text,
                                    marker_buffer_t* output);

/**
 * Escape HTML entities in text
 * @param text Input text
 * @param output Output buffer
 * @param output_size Size of output buffer
 * @return Result code
 */
marker_result_t marker_escape_html(const char* text, char* output, size_t output_size);

/**
 * Validate Markdown syntax (check for common errors)
 * @param markdown Input Markdown
 * @param error_msg Buffer for error message (optional)
 * @param error_msg_size Size of error message buffer
 * @return true if valid, false if errors found
 */
bool marker_validate(const char* markdown, char* error_msg, size_t error_msg_size);

/* Legacy API compatibility (deprecated) */
void md_to_html(const char* markdown, char* html, size_t html_size, const char* css_file);
int  md_file_to_html_file(const char* input_filename, const char* output_filename,
                          const char* css_file);
int  md_files_to_html_files(const char** input_files, const char** output_files, int count,
                            const char* css_file);

#ifdef __cplusplus
}
#endif

#endif /* MARKER_H */
