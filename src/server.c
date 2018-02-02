#include "server.h"
#include "debug.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include "csapp.h"

/*
 * Parses the arguments passed from the command line.
 *
 * @param argc The number of arguments passed in from the command line.
 * @param argv A pointer to an array of the command line arguments.
 * @return A pointer to the parsed arguments.
 */
args_struct *parse_args(int argc, char *argv[]) {
    if (argc != 2 && argc != 4 && argc != 5) {
        debug("Failed");
        USAGE(argv[0], EXIT_FAILURE);
    }

    if(strcmp(argv[1], "-h") == 0) {
        USAGE(argv[0], EXIT_SUCCESS);
    }

    args_struct *args = malloc(sizeof(args_struct));
    if (args == NULL) {
        USAGE(argv[0], EXIT_FAILURE);
    }

    args->NUM_WORKERS = atoi(argv[1]);
    args->PORT_NUMBER = argv[2];
    args->MAX_ENTRIES = atoi(argv[3]);

    if (args->NUM_WORKERS <= 0 || args->MAX_ENTRIES <= 0) {
        exit(EXIT_FAILURE);
    }

    return args;
}

/*
 * Starts the server
 *
 * @param args A pointer to the arguemnts passed from the command line.
 */
void start_server(args_struct *args) {
    server_hashmap = create_map(args->MAX_ENTRIES, jenkins_one_at_a_time_hash, destroy_hash_function);
    server_queue = create_queue();

    pthread_t *threads = calloc(args->NUM_WORKERS, sizeof(pthread_t));
    for(int i = 0; i < args->NUM_WORKERS; i++) {
        int x = pthread_create(&threads[i], NULL, worker_function, NULL);
        if (x != 0) {
            free(args);
            exit(EXIT_FAILURE);
        }
    }

    struct sockaddr_in clientAddress;
    socklen_t addressLength = sizeof(clientAddress);
    int listenfd = open_listenfd(args->PORT_NUMBER);
    while(1) {
        int connection = accept(listenfd, (struct sockaddr *)&clientAddress, &addressLength);
        if (connection < 0) {
            debug("Error Initiating connection");
            continue;
        }
        int *cfd = malloc(sizeof(int));
        *cfd = connection;
        enqueue(server_queue, cfd);
    }

    // Kill all threads after they are done with their jobs
    for (int i = 0; i < args->NUM_WORKERS; i++) {
        pthread_join(threads[i], NULL);
    }

    // clean up
    invalidate_queue(server_queue, destroy_queue_function);
    invalidate_map(server_hashmap);

    free(args);
    exit(EXIT_SUCCESS);
}

/*
 * Destroys the hash function for a given element
 *
 * @param key The key for an element in the hashmap
 * @param val The value of an element in the hashmap
 */
void destroy_hash_function(map_key_t key, map_val_t val) {
    free(key.key_base);
    free(val.val_base);
}

/*
 * Free's the memory allocated for a queue
 *
 * @param queue The queue
 */
void destroy_queue_function(void* queue) {
    free(queue);
}

bool isKeyValid(request_header_t request_header, response_header_t *response_header) {
    debug("request_header KEY%d", request_header.key_size);
    debug("request_header VAL %d", request_header.value_size);
    if (request_header.key_size >= MIN_KEY_SIZE && request_header.key_size <= MAX_KEY_SIZE) {
        return true;
    }

    response_header->response_code = BAD_REQUEST;
    return false;

}
bool isValValid(request_header_t request_header, response_header_t *response_header) {
    if (request_header.value_size >= MIN_VALUE_SIZE && request_header.value_size <= MAX_VALUE_SIZE) {
        return true;
    }

    response_header->response_code = BAD_REQUEST;
    return false;
}

int readNBytes(int client_fd, void* myBuffer, int bytesToRead) {
    int i = 0;
    char *bufferPtr = myBuffer;
    int leftToRead = bytesToRead;

    while (leftToRead > i) {
        int x = read(client_fd, bufferPtr, leftToRead);
        if (x < i) {
            if (errno== EINTR) {
                x = i;
            } else if (x < i) {
                return -1;
            }
        } else if (x == i) {
            return (bytesToRead-leftToRead);
        }
        leftToRead = leftToRead - x;
        bufferPtr = bufferPtr + x;
    }
    return (bytesToRead-leftToRead);
}

int writeNBytes(int client_fd, void* myBuffer, int bytesToRead) {
    int i = 0;
    char *bufferPtr = myBuffer;
    int leftToRead = bytesToRead;
    int x;

    while (leftToRead > i) {
        x = write(client_fd, bufferPtr, leftToRead);
        if (x < i) {
            if (errno== EINTR) {
                x = i;
            } else{
                return -1;
            }
        } else if (x == i) {
            return (bytesToRead-leftToRead);
        }
        leftToRead = leftToRead - x;
        bufferPtr = bufferPtr + x;
    }
    return bytesToRead;
}


void *worker_function() {
    while (1) {
        // get client file descriptor
        int *cfd = dequeue(server_queue);
        // if nothing in queue, try again
        if (cfd == NULL) {
            continue;
        }
        // client fd is valid and get the request and responce headers
        request_header_t request_header;
        response_header_t response_header;
        map_val_t map_value = MAP_VAL(NULL, 0);
        map_node_t map_node;

        int client_fd = *cfd;
        // first we try to read for the header
        if (readNBytes(client_fd, &request_header, sizeof(request_header)) < 0) {
            response_header.response_code = BAD_REQUEST;
            response_header.value_size = 0;
        } else {
            if (request_header.request_code == PUT) {
                debug("Works");
                debug("KEY VALID %d", isKeyValid(request_header, &response_header));
                debug("VALUE %d", isValValid(request_header, &response_header));
                if (isKeyValid(request_header, &response_header) && isValValid(request_header, &response_header)) {
                    void *key = malloc(request_header.key_size);
                    void *value = malloc(request_header.value_size);


                    // read and check if the key is valid
                    if (readNBytes(client_fd, key, request_header.key_size) < 0) {
                        response_header.response_code = BAD_REQUEST;
                        response_header.value_size = 0;
                    }

                    // read and check if the key is valid
                    if (readNBytes(client_fd, value, request_header.value_size) < 0) {
                        response_header.response_code = BAD_REQUEST;
                        response_header.value_size = 0;
                    }
                    debug("Key From Client: %s", (char*)key);
                    debug("Value From Client: %s", (char*)value);
                    // fullfill the PUT request
                    if (response_header.response_code != BAD_REQUEST) {
                        if (put(server_hashmap, MAP_KEY(key, request_header.key_size), MAP_VAL(value, request_header.value_size), true)) {
                            response_header.response_code = OK;
                            response_header.value_size = 0;
                        } else {
                            response_header.response_code = BAD_REQUEST;
                            response_header.value_size = 0;
                        }
                    }

                }
            } else if (request_header.request_code == GET) {
                if (isKeyValid(request_header, &response_header)) {
                    // MALLOC KEY
                    void *key = malloc(request_header.key_size);

                    // read the key
                    if (readNBytes(client_fd, key, request_header.key_size) < 0) {
                        response_header.response_code = BAD_REQUEST;
                        response_header.value_size = 0;
                    }

                    // fullfill the GET request
                    if(response_header.response_code != BAD_REQUEST) {
                        debug("Start Get");
                        map_value = get(server_hashmap, MAP_KEY(key, request_header.key_size));
                        debug("End GET");
                        if (map_value.val_base != NULL && map_value.val_len != 0) {
                            response_header.response_code = OK;
                            response_header.value_size = map_value.val_len;
                        } else {
                            // couldn't find the element in the hash map
                            response_header.response_code = NOT_FOUND;
                            response_header.value_size = 0;
                        }
                    }

                }
                debug("GET DONE");

            } else if (request_header.request_code == EVICT) {
                if(isKeyValid(request_header, &response_header)) {
                    // MALLOC KEY
                    void *key = malloc(request_header.key_size);

                    // read the key
                    if (readNBytes(client_fd, key, request_header.key_size) < 0) {
                        response_header.response_code = BAD_REQUEST;
                        response_header.value_size = 0;
                    }

                    if (response_header.response_code != BAD_REQUEST) {
                        map_node = delete(server_hashmap, MAP_KEY(key, request_header.key_size));
                        if (map_node.key.key_base != NULL || map_node.key.key_len != 0) {
                            response_header.response_code = OK;
                            response_header.value_size = 0;
                        } else {
                            // couldn't find the element in the hash map
                            response_header.response_code = NOT_FOUND;
                            response_header.value_size = 0;
                        }
                    }

                }

            } else if (request_header.request_code == CLEAR) {
                if (clear_map(server_hashmap) == false) {
                    response_header.response_code = BAD_REQUEST;
                    response_header.value_size = 0;
                } else {
                    response_header.response_code = OK;
                    response_header.value_size = 0;
                }
            } else {
                response_header.response_code = UNSUPPORTED;
                response_header.value_size = 0;
            }
        }
        // end if

        writeNBytes(client_fd, &response_header, sizeof(response_header));
        if (map_value.val_len != 0 && map_value.val_base != NULL) {
            writeNBytes(client_fd, map_value.val_base, map_value.val_len);
        }
        close(client_fd);
        free(cfd);
    }

}
