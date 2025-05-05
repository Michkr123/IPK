#ifndef TCP_CHAT_CLIENT_H
#define TCP_CHAT_CLIENT_H

#include "ChatClient.h"
#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstdlib>

/**
 * TCPChatClient is a concrete implementation of ChatClient for the TCP protocol.
 * It encapsulates all TCP-specific functions such as connecting, sending,
 * and receiving messages, and checking for replies.
 */
class TCPChatClient : public ChatClient {
public:
    /**
     * Constructor for TCPChatClient.
     *
     * @param hostname The server hostname.
     * @param port The server port.
     */
    TCPChatClient(const std::string &hostname, uint16_t port);

    /**
     * Destructor for TCPChatClient.
     */
    virtual ~TCPChatClient();

    /**
     * Connects to the server using TCP.
     * This will create a TCP socket and perform the connect() call.
     */
    virtual void connectToServer() override;

    /**
     * Authenticates the client with the server.
     *
     * @param username The username.
     * @param secret The secret or password.
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
     * Sends a BYE message to terminate the connection.
     */
    virtual void bye() override;

    /**
     * Listens for incoming messages on the TCP connection.
     * This function should run in its own thread.
     */
    virtual void listen() override;

private:
    /**
     * Sends a message using TCP.
     *
     * @param message The message string to send.
     */
    void sendTCP(const std::string &message);

    /**
     * Checks for a reply from the server after sending a command.
     * This function implements a timeout mechanism for waiting on a reply.
     */
    void checkReply();

    /**
     * Handles malformed messages error
     */
    void malformedMessage();

    bool replyReceived_ = false;
};

#endif // TCP_CHAT_CLIENT_H