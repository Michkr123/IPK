#ifndef UDP_CHAT_CLIENT_H
#define UDP_CHAT_CLIENT_H

#include "ChatClient.h"
#include <vector>
#include <cstdint>
#include <string>
#include <sys/mman.h>

/**
 * UDPChatClient is a concrete implementation of ChatClient for the UDP protocol.
 * It encapsulates all UDP-specific functions such as sending/receiving messages,
 * confirmation handling, and reply checking.
 *
 * This implementation uses shared memory (via mmap) for arrays of message IDs.
 */
class UDPChatClient : public ChatClient {
public:
    /**
     * Constructor for UDPChatClient.
     *
     * @param hostname The server hostname.
     * @param port The server port.
     */
    UDPChatClient(const std::string &hostname, uint16_t port, int retry_count, int timeout);

    /**
     * Destructor for UDPChatClient.
     */
    virtual ~UDPChatClient();

    // Overridden virtual methods from ChatClient:

    /**
     * Connects to the server.
     * For UDP, the OS automatically handles binding.
     */
    virtual void connectToServer() override;

    /**
     * Authenticates with the server.
     *
     * @param username The username.
     * @param secret The password/secret.
     * @param displayName The display name.
     */
    virtual void auth(const std::string &username,
                      const std::string &secret,
                      const std::string &displayName) override;

    /**
     * Joins a channel.
     *
     * @param channel The channel to join.
     */
    virtual void joinChannel(const std::string &channel) override;

    /**
     * Sends a regular chat message.
     *
     * @param message The message content.
     */
    virtual void sendMessage(const std::string &message) override;

    /**
     * Sends an error message.
     *
     * @param error The error description.
     */
    virtual void sendError(const std::string &error) override;

    /**
     * Sends a BYE message to close the connection.
     */
    virtual void bye() override;

    /**
     * Listens for incoming UDP messages. This will be run in a separate thread.
     */
    virtual void listen() override;

private:
    /**
     * Sends a UDP packet using the internal socket.
     *
     * @param buffer The buffer containing the message.
     */
    void sendUDP(const std::vector<uint8_t> &buffer);

    /**
     * Sends a confirmation message for a given reference message ID.
     *
     * @param refMessageID The message ID to confirm.
     */
    void confirm(uint16_t refMessageID);

    /**
     * Checks for a reply corresponding to the specified messageID.
     * This function implements a timeout mechanism (e.g., 5 seconds) using fork.
     *
     * @param messageID The message ID to check.
     */
    void checkReply(uint16_t messageID);

    // Shared memory arrays to track message confirmations and replies.
    std::vector<uint16_t> replyIDs;
    std::vector<uint16_t> confirmIDs;
    std::vector<uint16_t> seenIDs;


    // Number of extra times to retry sending if confirmation is not received.
    int retry_count_;
    // Maximum time (in milliseconds) to wait for confirmation on each attempt.
    int timeout_;
};

#endif // UDP_CHAT_CLIENT_H
