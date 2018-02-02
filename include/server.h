#ifndef SERVER_H
#define SERVER_H

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "utils.h"
#include "cream.h"

typedef struct args_struct{
int NUM_WORKERS;
char *PORT_NUMBER;
int MAX_ENTRIES;
} args_struct;

#define USAGE(prog_name, exitcode)                                                       \
  do {                                                                         \
    fprintf(stderr,                                                            \
            "\n%s [-h] NUM_WORKERS PORT_NUMBER MAX_ENTTRIES \n"                \
            "\n"                                                               \
            "-h                 Displays this help menu and returns EXIT_SUCCESS.\n"                          \
            "NUM_WORKERS        The number of worker threads used to service requests.\n"              \
            "PORT_NUMBER        Port number to listen on for incoming connections.\n"                                   \
            "MAX_ENTRIES        The maximum number of entries that can be stored in `cream`'s underlying data store.\n", \
            (prog_name));                                                      \
    exit(exitcode);                                                            \
  } while (0)

args_struct *parse_args(int argc, char *argv[]);
void start_server(args_struct *args);
queue_t *server_queue;
hashmap_t *server_hashmap;
void destroy_hash_function(map_key_t key, map_val_t val);
void destroy_queue_function(void* queue);
void *worker_function();
int readNBytes(int client_fd, void* myBuffer, int bytesToRead);
int writeNBytes(int client_fd, void* myBuffer, int bytesToRead);
bool isKeyValid(request_header_t request_header, response_header_t *response_header);
bool isValValid(request_header_t request_header, response_header_t *response_header);

#endif