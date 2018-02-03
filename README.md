# HashMap

This application is a multithreaded network hashmap.

./cream [-h] NUM_WORKERS PORT_NUMBER MAX_ENTRIES
-h            Displayed this help menu and returns EXIT_SUCCESS.
NUM_WORKERS   The number of worker threads used to service requests.
PORT_NUMBER   Port number to listen on for incomming connections.
MAX_ENTRIES   The maximum number of entries that can be stored in `creams`'s underlying data store.

After you launch the server, you can download and make the cream (Cache Rules Everything Around Me) client to send requests to the server.
