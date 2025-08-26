#include "iris.h"
#include <arpa/inet.h> // for INET_ADDRSTRLEN
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Clap solves this
int main(int argc, char *argv[]) {
  char address[INET_ADDRSTRLEN] = "0.0.0.0";
  char directory[IRIS_MAX_PATH_SIZE] = ".";
  int port = 8000;

  for (int i = 1; i < argc; ++i) {
    if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
      fprintf(stderr, "Usage: %s [-b ADDRESS] [-d DIRECTORY] [port]\n",
              argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
      strncpy(address, argv[++i], sizeof(address) - 1);
    } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
      strncpy(directory, argv[++i], sizeof(directory) - 1);
    } else {
      port = atoi(argv[i]);
    }
  }

  return iris_start(address, directory, port);
}
