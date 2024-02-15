#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <fstream>

// Constants
#define MAX_MESSAGE_SIZE 4096
#define MAX_FILE_SIZE 1048576 // 1MB

// Global Variables
std::vector<std::vector<int>> rooms; // Vector of vectors to store clients in different rooms
pthread_mutex_t roomMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t messageQueueMutex = PTHREAD_MUTEX_INITIALIZER;

// Message Queue Structure
struct Message {
    long messageType;
    char message[MAX_MESSAGE_SIZE];
};

// Function Prototypes
void* handleClient(void* arg);
void sendMessageToRoom(int roomID, const char* message, int senderSocket);
void* receiveMessages(void* arg);
bool promptFileTransfer(int clientSocket, const char* fileName, int fileSize);

int main() {
    // Initialize rooms vector
    rooms.resize(100); // Assume 100 rooms for simplicity

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    // Server address setup
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    // Binding the server socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Bind failed.\n";
        close(serverSocket);
        return 1;
    }

    // Listening for incoming connections
    if (listen(serverSocket, SOMAXCONN) == -1) {
        std::cerr << "Listen failed.\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "Server is listening on port 8081...\n";

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket == -1) {
            std::cerr << "Accept failed.\n";
            close(serverSocket);
            return 1;
        }

        pthread_t thread;
        if (pthread_create(&thread, nullptr, handleClient, &clientSocket) != 0) {
            std::cerr << "Failed to create thread.\n";
            close(clientSocket);
        }
        pthread_detach(thread);
    }

    close(serverSocket);
    return 0;
}

// Client Handling Function
void* handleClient(void* arg) {
    int clientSocket = *((int*)arg);
    char buffer[MAX_MESSAGE_SIZE];

    // Client room join handling
    int roomID;
    recv(clientSocket, &roomID, sizeof(roomID), 0);

    pthread_mutex_lock(&roomMutex);
    rooms[roomID].push_back(clientSocket);
    pthread_mutex_unlock(&roomMutex);

    // Broadcast client joining message
    std::string joinMessage = "CLIENT " + std::to_string(clientSocket) + " JOINED ROOM " + std::to_string(roomID);
    sendMessageToRoom(roomID, joinMessage.c_str(), clientSocket);

    // Receive and broadcast messages
    pthread_t receiveThread;
    if (pthread_create(&receiveThread, nullptr, receiveMessages, &clientSocket) != 0) {
        std::cerr << "Failed to create receive thread.\n";
        close(clientSocket);
        return nullptr;
    }

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            break;
        }

        buffer[bytesReceived] = '\0';
        // Check if the client wants to change the room
        if (strncmp(buffer, "/join", 5) == 0) {
            int newRoomID = atoi(buffer + 6);
            pthread_mutex_lock(&roomMutex);
            auto it = std::find(rooms[roomID].begin(), rooms[roomID].end(), clientSocket);
            if (it != rooms[roomID].end()) {
                rooms[roomID].erase(it);
            }
            rooms[newRoomID].push_back(clientSocket);
            pthread_mutex_unlock(&roomMutex);
            roomID = newRoomID;
            std::string joinMessage = "CLIENT " + std::to_string(clientSocket) + " JOINED ROOM " + std::to_string(roomID);
            sendMessageToRoom(roomID, joinMessage.c_str(), clientSocket);
        } else {
            sendMessageToRoom(roomID, buffer, clientSocket);
        }
    }

    // Client disconnection handling
    pthread_cancel(receiveThread);
    close(clientSocket);

    pthread_mutex_lock(&roomMutex);
    auto it = std::find(rooms[roomID].begin(), rooms[roomID].end(), clientSocket);
    if (it != rooms[roomID].end()) {
        rooms[roomID].erase(it);
    }
    pthread_mutex_unlock(&roomMutex);

    std::string disconnectMessage = "CLIENT " + std::to_string(clientSocket) + " DISCONNECTED";
    sendMessageToRoom(roomID, disconnectMessage.c_str(), clientSocket);

    pthread_exit(nullptr);
}

// Message sending function to all clients in a room except the sender
void sendMessageToRoom(int roomID, const char* message, int senderSocket) {
    pthread_mutex_lock(&roomMutex);
    for (int clientSocket : rooms[roomID]) {
        if (clientSocket != senderSocket) {
            send(clientSocket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&roomMutex);
}

// Message receiving function
void* receiveMessages(void* arg) {
    int clientSocket = *((int*)arg);
    char buffer[MAX_MESSAGE_SIZE];

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            break;
        }
        buffer[bytesReceived] = '\0';
        std::cout << "Client " << clientSocket << ": " << buffer << std::endl;
    }

    close(clientSocket);
    pthread_exit(nullptr);
}
