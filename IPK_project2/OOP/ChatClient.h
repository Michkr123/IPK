#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include <string>
#include <netinet/in.h>
#include <cstdint>
#include <thread>

/**
 * Abstract base class ChatClient.
 * This class defines common properties (hostname, port, socket, state, messageID)
 * and a common interface for methods that will be implemented by derived classes 
 * (e.g., UDPChatClient and TCPChatClient).
 */
class ChatClient {
public:
    /**
     * Constructor that initializes the hostname and port.
     *
     * @param hostname The server hostname.
     * @param port The server port.
     */
    ChatClient(const std::string &hostname, uint16_t port);

    /**
     * Virtual destructor.
     */
    virtual ~ChatClient();

    // Pure virtual functions which must be implemented by derived classes:
    
    /**
     * Connects to the server.
     * For TCP, this will perform the connect() call, while for UDP this may be a no-op.
     */
    virtual void connectToServer() = 0;

    /**
     * Authenticates the client.
     *
     * @param username The username.
     * @param secret The secret or password.
     * @param displayName The display name.
     */
    virtual void auth(const std::string &username,
                      const std::string &secret,
                      const std::string &displayName) = 0;

    /**
     * Joins a channel.
     *
     * @param channel The channel to join.
     */
    virtual void joinChannel(const std::string &channel) = 0;

    /**
     * Sends a regular message.
     *
     * @param message The message content.
     */
    virtual void sendMessage(const std::string &message) = 0;

    /**
     * Sends an error message.
     *
     * @param error The error description.
     */
    virtual void sendError(const std::string &error) = 0;

    /**
     * Sends a BYE message to terminate the connection.
     */
    virtual void bye() = 0;

    /**
     * Listens for incoming messages.
     * This function will be executed in its own thread.
     */
    virtual void listen() = 0;

    /**
     * Starts the listener in a new thread.
     */
    virtual void startListener();

    /**
     * Gracefully ends the connection.
     */
    void gracefullEnd();

    /**
     * Returns the current state of the connection.
     *
     * @return The state, e.g., "start", "auth", "open", "end".
     */
    std::string getState() const;

    /**
     * Sets the current state of the connection.
     *
     * @param newState The new state.
     */
    void setState(const std::string &newState);

    /**
     * Returns the next unique message ID and increments it.
     *
     * @return The next message ID.
     */
    uint16_t getNextMessageID();

    /**
     * Changes the name that is displayed.
     *
     * @param displayName The new display name.
     */
    void rename(const std::string &displayName);

protected:
    std::string hostname_;          // Server hostname.
    uint16_t port_;                 // Server port.
    int sockfd_;                    // Socket file descriptor.
    struct sockaddr_in serverAddr_; // Server address structure.
    std::string state_;             // Connection state (e.g., "start", "auth", "open", "end").
    uint16_t messageID_;            // Last used message ID.
    std::string displayName_;

};

#endif // CHAT_CLIENT_H
