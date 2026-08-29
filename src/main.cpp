#include "router/Router.h"

/**
 * Entry point of the router. Constructs the Router (the composition root)
 * and starts its receive loop.
 * @return 0 on completion, otherwise the program is terminated by an error.
 */
int main() {
    Router router;
    router.run();

    return 0;
}