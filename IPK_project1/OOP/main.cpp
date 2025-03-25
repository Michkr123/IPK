#include "PortScanner.hpp"
#include <csignal>
#include <iostream>

PortScanner scanner;

void signal_handler(int) {
    std::exit(0);
}

int main(int argc, char *argv[]) {
    std::signal(SIGINT, signal_handler);

    scanner.parse_arguments(argc, argv);
    scanner.resolve_target_ip();
    scanner.run();

    return 0;
}