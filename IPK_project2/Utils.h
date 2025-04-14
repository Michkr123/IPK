#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <netinet/in.h>


struct Options {
    std::string protocol;
    std::string hostname;

    uint16_t port = 4567;
    uint16_t timeout = 250;
    uint8_t retry_count = 3;
};

/**
 * Prints out help for program execute.
 */
void help();

/**
 * Prints out user commands.
 */
void commandHelp();

/**
 * Parses the command-line arguments into an Options struct.
 * Supported arguments:
 * 
 *   -t <protocol>    (required; e.g., udp or tcp)
 * 
 *   -s <hostname/IP> (required; server's hostname or IP)
 * 
 *   -p <port>        (optional; default: 4567)
 * 
 *   -d <timeout>     (optional; timeout in milliseconds)
 * 
 *   -r <retry_count> (optional; maximum number of retransmissions)
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return Options struct populated with the parsed values.
 */
Options parseArguments(int argc, char *argv[]);

/**
 * Splits the given string by whitespace.
 *
 * @param str The input string.
 * @return A vector of tokens.
 */
std::vector<std::string> split(const std::string &str);

/**
 * Checks whether the string contains only alphanumeric characters,
 * hyphen ('-'), or underscore ('_').
 *
 * @param str The string to check.
 * @return true if valid, false otherwise.
 */
bool isValidString(const std::string &str);

/**
 * Checks whether all characters in the string are printable (from 0x21 to 0x7E).
 *
 * @param str The string to check.
 * @return true if all characters are printable, false otherwise.
 */
bool isPrintableChar(const std::string &str);

/**
 * Checks whether the message is valid.
 * It allows newline (0x0A) and printable characters (0x20 to 0x7E) for other characters.
 *
 * @param message The message content.
 * @return true if valid, false otherwise.
 */
bool isValidMessage(const std::string &message);

#endif // UTILS_H