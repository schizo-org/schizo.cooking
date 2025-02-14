#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define MAX_PATH_SIZE 512
#define MAX_HEADER_SIZE 1024
#define MAX_METHOD_SIZE 16
#define MAX_VERSION_SIZE 16

typedef struct {
  const char *extension;
  const char *mime_type;
} mime_entry;

mime_entry mime_types[] = {{".html", "text/html"},
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

const char *get_mime_type(const char *path) {
  const char *ext = strrchr(path, '.');
  if (ext) {
    for (mime_entry *entry = mime_types; entry->extension; ++entry) {
      if (strcasecmp(ext, entry->extension) == 0) {
        return entry->mime_type;
      }
    }
  }
  return "application/octet-stream";
}

void get_http_date(char *buffer, size_t buffer_size) {
  time_t now = time(NULL);
  struct tm tm_now;
  gmtime_r(&now, &tm_now);
  strftime(buffer, buffer_size, "%a, %d %b %Y %H:%M:%S GMT", &tm_now);
}

void send_error_response(int client_fd, int status_code, const char *message) {
  char date[128];
  get_http_date(date, sizeof(date));
  char body[256];
  snprintf(body, sizeof(body),
           "<html><head><title>%d %s</title></head>"
           "<body><h1>%d %s</h1></body></html>",
           status_code, message, status_code, message);
  size_t body_length = strlen(body);

  char header[MAX_HEADER_SIZE];
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

void send_file(const char *path, int client_fd) {
  char date[128];
  get_http_date(date, sizeof(date));
  FILE *file = fopen(path, "rb");
  if (!file) {
    send_error_response(client_fd, 404, "Not Found");
    return;
  }
  struct stat st;
  if (stat(path, &st) != 0) {
    fclose(file);
    send_error_response(client_fd, 404, "Not Found");
    return;
  }
  const char *mime_type = get_mime_type(path);
  char header[MAX_HEADER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.0 200 OK\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %ld\r\n"
           "Date: %s\r\n"
           "Server: SimpleHTTP/1.0\r\n\r\n",
           mime_type, st.st_size, date);
  send(client_fd, header, strlen(header), 0);

  char buffer[BUFFER_SIZE];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
    send(client_fd, buffer, bytes_read, 0);
  }
  fclose(file);
}

void send_directory_listing(const char *path, int client_fd) {
  char date[128];
  get_http_date(date, sizeof(date));
  char header[MAX_HEADER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.0 200 OK\r\n"
           "Content-Type: text/html\r\n"
           "Date: %s\r\n"
           "Server: SimpleHTTP/1.0\r\n\r\n",
           date);
  send(client_fd, header, strlen(header), 0);

  char buffer[BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer),
           "<html><head><title>Directory listing for %s</title></head>"
           "<body><h1>Directory listing for %s</h1><ul>",
           path, path);
  send(client_fd, buffer, strlen(buffer), 0);

  DIR *dir = opendir(path);
  if (!dir) {
    send_error_response(client_fd, 500, "Internal Server Error");
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

// Validate the requested path:
//  - Disallow ".." to prevent directory traversal.
//  - Ensure that the path starts with '/'.
// This should be a very basic way of catching directory traversal
// attempts, but I'm sure there is something I'm missing here.
int sanitize_path(const char *base_dir, const char *requested_path,
                  char *full_path, size_t size) {
  if (strstr(requested_path, "..") || requested_path[0] != '/') {
    return 0; // Reject invalid or traversal paths.
  }

  // Build full path as base_dir + requested_path.
  size_t required_size =
      snprintf(full_path, size, "%s%s", base_dir, requested_path);
  if (required_size >= size) {
    return 0; // Path too long.
  }
  return 1;
}

int main(int argc, char *argv[]) {
  char address[INET_ADDRSTRLEN] = "0.0.0.0";
  char directory[MAX_PATH_SIZE] = ".";
  int port = 8000;

  // Command-line argument parsing
  // Clap solves this...
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      fprintf(stderr, "usage: %s [-b ADDRESS] [-d DIRECTORY] [port]\n",
              argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-b") == 0) {
      strncpy(address, argv[++i], sizeof(address) - 1);
    } else if (strcmp(argv[i], "-d") == 0) {
      strncpy(directory, argv[++i], sizeof(directory) - 1);
    } else {
      port = atoi(argv[i]);
    }
  }
  if (chdir(directory) == -1) {
    perror("chdir");
    return 1;
  }
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket");
    return 1;
  }

  // Set SO_REUSEADDR to allow quick restart
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;

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
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
      close(client_fd);
      continue;
    }
    buffer[bytes_read] = '\0';
    printf("Request:\n%s\n", buffer);

    char method[MAX_METHOD_SIZE] = {0};
    char path[MAX_PATH_SIZE] = {0};
    char version[MAX_VERSION_SIZE] = {0};

    int tokens = sscanf(buffer, "%15s %511s %15s", method, path, version);
    if (tokens != 3) {
      send_error_response(client_fd, 400, "Bad Request");
      close(client_fd);
      continue;
    }

    // Method must be GET, and the path must start with '/'
    if (strcasecmp(method, "GET") != 0 || path[0] != '/') {
      send_error_response(client_fd, 405, "Method Not Allowed");
      close(client_fd);
      continue;
    }

    char full_path[MAX_PATH_SIZE];
    if (!sanitize_path(directory, path, full_path, sizeof(full_path))) {
      send_error_response(client_fd, 403, "Forbidden");
      close(client_fd);
      continue;
    }

    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        char index_path[MAX_PATH_SIZE];
        if (snprintf(index_path, sizeof(index_path), "%s/index.html",
                     full_path) >= (int)sizeof(index_path)) {
          send_error_response(client_fd, 414, "Request-URI Too Long");
          close(client_fd);
          continue;
        }
        if (stat(index_path, &st) == 0 && S_ISREG(st.st_mode)) {
          send_file(index_path, client_fd);
        } else {
          send_directory_listing(full_path, client_fd);
        }
      } else if (S_ISREG(st.st_mode)) {
        send_file(full_path, client_fd);
      } else {
        send_error_response(client_fd, 403, "Forbidden");
      }
    } else {
      send_error_response(client_fd, 404, "Not Found");
    }
    close(client_fd);
  }
  close(server_fd);
  return 0;
}
