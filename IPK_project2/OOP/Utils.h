#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <netinet/in.h>


struct Options {
    std::string protocol;
    std::string hostname;
    std::string username;
    std::string secret;
    std::string displayName;

    uint16_t port = 4567;
    uint16_t timeout = 250;
    uint16_t messageID = 0;
    uint8_t retry_count = 3;

    int32_t sockfd = -1;
    sockaddr_in serverAddr;

    std::string state = "start";

    std::vector<uint16_t> seenMessagesID;
    std::vector<uint16_t> messageReplysID;
};

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

/**
 * Resolves a hostname and fills in a sockaddr_in structure.
 *
 * @param hostname The hostname to resolve.
 * @param port The port number.
 * @param serverAddr The sockaddr_in structure to fill.
 */
void resolve_hostname(const std::string &hostname, uint16_t port, struct sockaddr_in &serverAddr);

#endif // UTILS_H
