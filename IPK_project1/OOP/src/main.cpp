#include "PortScanner.hpp"
#include <csignal>

PortScanner scanner;

/**
 * @brief Signal handler to gracefully exit on SIGINT (Ctrl+C).
 * 
 * @param signal The signal number (unused).
 */
void signal_handler(int) {
    std::exit(0);
}

/**
 * @brief Entry point of the program. Parses arguments and initiates port scanning.
 * 
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return int Exit status code.
 */
int main(int argc, char *argv[]) {
    std::signal(SIGINT, signal_handler);

    scanner.parse_arguments(argc, argv);
    scanner.get_source_ip();
    scanner.run();

    return 0;
}