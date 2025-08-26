#define _POSIX_C_SOURCE 200112L
#include "iris.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static iris_mime_entry mime_types[] = {{".html", "text/html"},
                                       {".htm", "text/html"},
                                       {".css", "text/css"},
                                       {".js", "application/javascript"},
                                       {".json", "application/json"},
                                       {".png", "image/png"},
                                       {".jpg", "image/jpeg"},
                                       {".jpeg", "image/jpeg"},
                                       {".gif", "image/gif"},
                                       {".txt", "text/plain"},
                                       {NULL, "application/octet-stream"}};

const char *iris_get_mime_type(const char *path) {
  const char *ext = strrchr(path, '.');
  if (ext) {
    for (iris_mime_entry *entry = mime_types; entry->extension; ++entry) {
      if (strcasecmp(ext, entry->extension) == 0) {
        return entry->mime_type;
      }
    }
  }

  return "application/octet-stream";
}

void iris_get_http_date(char *buffer, size_t buffer_size) {
  time_t now = time(NULL);
  struct tm tm_now;
#if defined(_POSIX_VERSION)
  gmtime_r(&now, &tm_now);
#else
  struct tm *tmptr = gmtime(&now);
  if (tmptr)
    tm_now = *tmptr;
  else
    memset(&tm_now, 0, sizeof(tm_now));
#endif
  strftime(buffer, buffer_size, "%a, %d %b %Y %H:%M:%S GMT", &tm_now);
}

void iris_send_error_response(int client_fd, int status_code,
                              const char *message) {
  char date[128];
  iris_get_http_date(date, sizeof(date));
  char body[256];
  snprintf(body, sizeof(body),
           "<html><head><title>%d %s</title></head>"
           "<body><h1>%d %s</h1></body></html>",
           status_code, message, status_code, message);
  size_t body_length = strlen(body);

  char header[IRIS_MAX_HEADER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.0 %d %s\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: %lu\r\n"
           "Date: %s\r\n"
           "Server: SimpleHTTP/1.0\r\n\r\n",
           status_code, message, body_length, date);
  send(client_fd, header, strlen(header), 0);
  send(client_fd, body, body_length, 0);
}

void iris_send_file(const char *path, int client_fd) {
  char date[128];
  iris_get_http_date(date, sizeof(date));

  FILE *file = fopen(path, "rb");
  if (!file) {
    iris_send_error_response(client_fd, 404, "Not Found");
    return;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    fclose(file);
    iris_send_error_response(client_fd, 404, "Not Found");
    return;
  }

  const char *mime_type = iris_get_mime_type(path);
  char header[IRIS_MAX_HEADER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.0 200 OK\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %ld\r\n"
           "Date: %s\r\n"
           "Server: SimpleHTTP/1.0\r\n\r\n",
           mime_type, st.st_size, date);
  send(client_fd, header, strlen(header), 0);

  char buffer[IRIS_BUFFER_SIZE];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    send(client_fd, buffer, bytes_read, 0);
  }
  fclose(file);
}

void iris_send_directory_listing(const char *path, int client_fd) {
  char date[128];
  iris_get_http_date(date, sizeof(date));
  char header[IRIS_MAX_HEADER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.0 200 OK\r\n"
           "Content-Type: text/html\r\n"
           "Date: %s\r\n"
           "Server: SimpleHTTP/1.0\r\n\r\n",
           date);
  send(client_fd, header, strlen(header), 0);

  char buffer[IRIS_BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer),
           "<html><head><title>Directory listing for %s</title></head>"
           "<body><h1>Directory listing for %s</h1><ul>",
           path, path);
  send(client_fd, buffer, strlen(buffer), 0);

  DIR *dir = opendir(path);
  if (!dir) {
    iris_send_error_response(client_fd, 500, "Internal Server Error");
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    snprintf(buffer, sizeof(buffer), "<li><a href=\"%s\">%s</a></li>",
             entry->d_name, entry->d_name);
    send(client_fd, buffer, strlen(buffer), 0);
  }

  closedir(dir);
  snprintf(buffer, sizeof(buffer), "</ul></body></html>");
  send(client_fd, buffer, strlen(buffer), 0);
}

int iris_sanitize_path(const char *base_dir, const char *requested_path,
                       char *full_path) {
  if (strcmp(requested_path, "/") == 0) {
    snprintf(full_path, IRIS_MAX_PATH_SIZE, "%s", base_dir);
    return 1;
  }

  // Reject if the requested path is not rooted
  if (requested_path[0] != '/') {
    return 0;
  }

  char absolute_base_dir[IRIS_MAX_PATH_SIZE];
  if (strcmp(base_dir, ".") == 0) {
    if (!getcwd(absolute_base_dir, sizeof(absolute_base_dir))) {
      perror("getcwd failed");
      return 0;
    }
    base_dir = absolute_base_dir;
  }

  char resolved_path[IRIS_MAX_PATH_SIZE];
  snprintf(resolved_path, sizeof(resolved_path), "%s%s", base_dir,
           requested_path);
  if (!realpath(resolved_path, full_path)) {
    perror("realpath failed");
    return 0;
  }

  size_t base_len = strlen(base_dir);
  if (base_len > 1 && base_dir[base_len - 1] == '/') {
    base_len--;
  }
  if (strncmp(full_path, base_dir, base_len) != 0) {
    printf("Path outside base directory: %s\n", full_path);
    return 0;
  }

  return 1;
}

int iris_start(const char *address, const char *directory, int port) {
  if (chdir(directory) == -1) {
    perror("chdir");
    return 1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket");
    return 1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0) {
    perror("inet_pton");
    close(server_fd);
    return 1;
  }

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("bind");
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 10) == -1) {
    perror("listen");
    close(server_fd);
    return 1;
  }

  printf("Serving HTTP on %s port %d (http://%s:%d/) ...\n", address, port,
         address, port);

  while (1) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
      perror("accept");
      continue;
    }

    char buffer[IRIS_BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
      close(client_fd);
      continue;
    }
    buffer[bytes_read] = '\0';
    printf("Request:\n%s\n", buffer);

    char method[IRIS_MAX_METHOD_SIZE] = {0};
    char path[IRIS_MAX_PATH_SIZE] = {0};
    char version[IRIS_MAX_VERSION_SIZE] = {0};

    int tokens = sscanf(buffer, "%15s %511s %15s", method, path, version);
    if (tokens != 3) {
      iris_send_error_response(client_fd, 400, "Bad Request");
      close(client_fd);
      continue;
    }

    // Only allow GET method and require the path to start with '/'
    if (strcasecmp(method, "GET") != 0 || path[0] != '/') {
      iris_send_error_response(client_fd, 405, "Method Not Allowed");
      close(client_fd);
      continue;
    }

    char full_path[IRIS_MAX_PATH_SIZE];
    if (!iris_sanitize_path(directory, path, full_path)) {
      iris_send_error_response(client_fd, 403, "Forbidden");
      close(client_fd);
      continue;
    }

    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        char index_path[IRIS_MAX_PATH_SIZE + 12]; // 12 for "/index.html"
        snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
        if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
          iris_send_file(index_path, client_fd);
        } else {
          iris_send_directory_listing(full_path, client_fd);
        }
      } else if (S_ISREG(st.st_mode)) {
        iris_send_file(full_path, client_fd);
      } else {
        iris_send_error_response(client_fd, 403, "Forbidden");
      }
    } else {
      iris_send_error_response(client_fd, 404, "Not Found");
    }

    close(client_fd);
  }

  close(server_fd);
  return 0;
}
