#ifndef IRIS_H
#define IRIS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#define IRIS_BUFFER_SIZE 4096
#define IRIS_MAX_PATH_SIZE 512
#define IRIS_MAX_HEADER_SIZE 1024
#define IRIS_MAX_METHOD_SIZE 16
#define IRIS_MAX_VERSION_SIZE 16

typedef struct {
  const char* extension;
  const char* mime_type;
} iris_mime_entry;

/*
 * Get the MIME type based on the file extension in the given path.
 *
 * @param path The file path.
 * @return A string representing the MIME type.
 */
const char* iris_get_mime_type(const char* path);

/*
 * Fill the provided buffer with the current HTTP date string.
 *
 * @param buffer The buffer to be filled.
 * @param buffer_size The size of the buffer.
 */
void iris_get_http_date(char* buffer, size_t buffer_size);

/*
 * Send an error HTTP response to the client.
 *
 * @param client_fd The client socket file descriptor.
 * @param status_code The HTTP status code.
 * @param message The status message (e.g., "Not Found").
 */
void iris_send_error_response(int client_fd, int status_code, const char* message);

/*
 * Send the contents of a file as an HTTP response.
 *
 * @param path The path to the file.
 * @param client_fd The client socket file descriptor.
 */
void iris_send_file(const char* path, int client_fd);

/*
 * Send an HTML directory listing of the given directory.
 *
 * @param path The directory path.
 * @param client_fd The client socket file descriptor.
 */
void iris_send_directory_listing(const char* fs_path, const char* url_path, int client_fd);

/*
 * Validate and sanitize the requested path against the base directory.
 *
 * @param base_dir The base directory from which files are served.
 * @param requested_path The HTTP requested path.
 * @param full_path Buffer where the full, sanitized path is stored.
 * @return 1 if the path is valid and within the base directory, 0 otherwise.
 */
int iris_sanitize_path(const char* base_dir, const char* requested_path, char* full_path);

/*
 * Start the Iris HTTP server.
 *
 *
 * @param address The address to bind to (e.g., "0.0.0.0").
 * @param directory The directory from which to serve files.
 * @param port The port number to listen on.
 * @return 0 on success, non-zero on error.
 */
int iris_start(const char* address, const char* directory, int port);

#ifdef __cplusplus
}
#endif

#endif /* IRIS_H */
