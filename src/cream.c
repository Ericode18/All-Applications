#include "cream.h"
#include "utils.h"
#include "server.h"
#include "debug.h"

int main(int argc, char *argv[]) {
    args_struct *args = parse_args(argc, argv);
    debug("Number of Workers: %d", args->NUM_WORKERS);
    debug("Number of Workers: %s", args->PORT_NUMBER);
    debug("Number of Workers: %d", args->MAX_ENTRIES);

    start_server(args);

    free(args);
    exit(0);
}
