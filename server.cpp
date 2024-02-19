#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <algorithm>
#include <condition_variable>

class SocketConnection {
private:
    int serverSocket;
    struct sockaddr_in serverAddress;

public:
    SocketConnection(int port) {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            reportError("Error creating socket");
        } else {
            serverAddress.sin_family = AF_INET;
            serverAddress.sin_addr.s_addr = INADDR_ANY;
            serverAddress.sin_port = htons(port);

            if (bind(serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
                reportError("Bind failed");
                close(serverSocket);
            }
        }
    }

    void reportError(const char* message) const {
        perror(message);
    }

    int listenConnection() const {
        if (listen(serverSocket, SOMAXCONN) == -1) {
            reportError("Listen failed");
            return -1;
        }
        return 0;
    }

    int acceptConnection(struct sockaddr_in &clientAddress) const {
        socklen_t clientAddressLength = sizeof(clientAddress);
        return accept(serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddressLength);
    }

    void sendData(int clientSocket, const char* buffer, size_t length, int flags) const {
        send(clientSocket, buffer, length, flags);
    }

    ssize_t receiveData(int clientSocket, char* buffer, size_t length, int flags) const {
        ssize_t receivedBytes = recv(clientSocket, buffer, length, flags);
        return receivedBytes;
    }

    void closeConnection(){
        close(serverSocket);
    }
};

class ChatMessage {
public:
    std::string content;
    std::string senderName;
    std::string filename;
    int senderSocket;
    int messageId;
};

class ChatRoom {
public:
    std::string name;
    std::thread roomThread;
    std::vector<int> clients;
    std::queue<ChatMessage> messageQueue;
    std::mutex roomMutex;
    std::condition_variable messageCondition;
    int nextMessageId = 0;

    explicit ChatRoom(std::string name) : name(std::move(name)) {
        roomThread = std::thread(&ChatRoom::broadcastMessages, this);
    }

    void addClient(int clientSocket) {
        std::lock_guard<std::mutex> lock(roomMutex);
        clients.push_back(clientSocket);
        std::cout << "CLIENT " << clientSocket << " JOINED ROOM " << name << std::endl;
    }

    void removeClient(int clientSocket) {
        std::lock_guard<std::mutex> lock(roomMutex);
        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
        std::cout << "CLIENT " << clientSocket << " LEFT THE ROOM " << name << std::endl;
    }

    void addMessageToQueue(const ChatMessage& message) {
        {
            std::lock_guard<std::mutex> lock(roomMutex);
            messageQueue.push(message);
        }
        messageCondition.notify_one();
    }

    std::string getName() const {
        return name;
    }

    void broadcastMessages() {
        while (true) {
            std::unique_lock<std::mutex> lock(roomMutex);
            messageCondition.wait(lock, [this]{ return !messageQueue.empty();});
            while (!messageQueue.empty()) {
                ChatMessage message = messageQueue.front();
                messageQueue.pop();
                if (message.content.find("SEND ") == 0){
                    struct stat fileInfo{};
                    std::cout << "Client " << message.senderSocket << " sends a file " << message.filename << std::endl;
                    lock.unlock();
                    for (int clientSocket : clients) {
                        if (clientSocket != message.senderSocket){
                            std::string askClient = "\nCLIENT " + message.senderName + " wants to send " + message.filename + " file which size is 1 MB, do you want to receive?";
                            send(clientSocket, askClient.c_str(), askClient.length(), 0);
                        }
                    }
                    lock.lock();
                } else {
                    lock.unlock();
                    for (int clientSocket : clients) {
                        if (clientSocket != message.senderSocket){
                            std::string messageContentName = "\n" + message.senderName + ": " + message.content;
                            send(clientSocket, messageContentName.c_str(), messageContentName.length(), 0);
                        }
                    }
                    lock.lock();
                }
            }
            if (clients.empty()){
                break;
            }
        }
    }
};

class FileManager {
public:
    static void copyFile(const std::string& sourcePath, const std::string& destinationPath, const int socket);
};

std::mutex fileMutex;

void FileManager::copyFile(const std::string& sourcePath, const std::string& destinationPath, const int socket) {
    std::ifstream file(sourcePath, std::ios::binary | std::ios::ate);

    if (file.is_open()) {
        std::ofstream outFile(destinationPath, std::ios::binary);

        if (!outFile.is_open()) {
            fileMutex.lock();
            std::cerr << "Failed to open file '" << destinationPath << "' for writing." << std::endl;
            fileMutex.unlock();
            const char *error = "File cannot be created.";
            send(socket, error, strlen(error), 0);
            return;
        }

        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        while (fileSize > 0) {
            char buffer[1024];
            file.read(buffer, sizeof(buffer));
            std::streamsize bytes = file.gcount();
            fileSize -= bytes;
            outFile.write(buffer, bytes);
        }

        const char *confirm = "File was saved successfully.";
        send(socket, confirm, strlen(confirm), 0);

        fileMutex.lock();
        std::cout << " CLIENT " << socket << " accepted and downloaded a file" << std::endl;
        fileMutex.unlock();
    } else {
        fileMutex.lock();
        std::cerr << "Failed to open file '" << sourcePath << "'" << std::endl;
        fileMutex.unlock();
        const char *error = "File not found or cannot be opened.";
        send(socket, error, strlen(error), 0);
    }
}



class ChatServer {
private:
    int port = 12341;
    sockaddr_in clientAddress;
    SocketConnection serverSocket;
    std::vector<std::thread> clientThreads;
    std::vector<std::unique_ptr<ChatRoom>> chatRooms;
    std::mutex chatRoomsMutex;
    std::string directoryForCopy;

    void listenSocket(){
        if (serverSocket.listenConnection() == -1) {
            return;
        } else {
            std::cout << "Server listening on port " << port << std::endl;

            while (true) {

                int clientSocket = serverSocket.acceptConnection(clientAddress);
                if (clientSocket == -1) {
                    serverSocket.reportError("Error accepting client connection");
                    break;
                }

                std::cout << "Accepted connection from " << inet_ntoa(clientAddress.sin_addr) << ":" << ntohs(clientAddress.sin_port) << std::endl;

                clientThreads.emplace_back(&ChatServer::handleCommunication, this, clientSocket);
            }
        }
    }

public:
    ChatServer() : serverSocket(port) {
        listenSocket();
    }

    ~ChatServer() {
        serverSocket.closeConnection();
        for (auto& thread : clientThreads) {
            thread.join();
        }
    }

    void setDirectories(std::string& clientName, std::string& clientFolderPath, std::string& serverFolderPath) {
        std::string baseFoldersPath = "./chat_app/chatapp_/";
        clientFolderPath = baseFoldersPath + clientName;
        serverFolderPath = baseFoldersPath + "server" + clientName.substr(1);

        if (!std::filesystem::exists(clientFolderPath)) {
            if (std::filesystem::create_directories(clientFolderPath)) {
                std::cout << "Created client directory: " << clientFolderPath << std::endl;
            } else {
                std::cerr << "Failed to create client directory: " << clientFolderPath << std::endl;
            }
        }

        if (!std::filesystem::exists(serverFolderPath)) {
            if (std::filesystem::create_directories(serverFolderPath)) {
                std::cout << "Created server directory: " << serverFolderPath << std::endl;
            } else {
                std::cerr << "Failed to create server directory: " << serverFolderPath << std::endl;
            }
        }
    }

    void getClientNameAndRoom(int clientSocket, std::string& clientName, std::string& roomName){
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Failed to read client's name.\n";
            close(clientSocket);
            return;
        }
        clientName = std::string(buffer, bytesRead);

        memset(buffer, 0, sizeof(buffer));
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Failed to read room name.\n";
            close(clientSocket);
            return;
        }
        roomName = std::string(buffer, bytesRead);

        std::cout << "CLIENT " << clientName << " join to the room: " << roomName << std::endl;
    }

    ChatRoom* findOrCreateRoom(const std::string& roomName) {
        std::lock_guard<std::mutex> lock(chatRoomsMutex);
        for (auto& room : chatRooms) {
            if (room->getName() == roomName) {
                return room.get();
            }
        }
        chatRooms.emplace_back(std::make_unique<ChatRoom>(roomName));
        return chatRooms.back().get();
    }

    void handleCommunication(int clientSocket) {
        std::string roomName;
        std::string clientName;
        getClientNameAndRoom(clientSocket, clientName, roomName);

        std::string clientFolderPath;
        std::string serverFolderPath;
        setDirectories(clientName, clientFolderPath, serverFolderPath);

        ChatRoom *room = findOrCreateRoom(roomName);
        room->addClient(clientSocket);
        std::string pathToFile;
        std::string pathToCopiedFile;

        while (true) {
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));
            ssize_t receivedBytes = serverSocket.receiveData(clientSocket, buffer, sizeof(buffer), 0);
            if (receivedBytes > 0) {
                std::string content(buffer, receivedBytes);
                if (content == "EXIT") {
                    room->removeClient(clientSocket);
                    std::cout << "Client " << clientSocket << " has left the room " << roomName << std::endl;
                    getClientNameAndRoom(clientSocket, clientName, roomName);
                } else if (content.find("YES ") == 0) {
                    std::string filename = content.substr(4);
                    pathToFile = directoryForCopy;
                    pathToCopiedFile = clientFolderPath + "/" + filename;
                    FileManager::copyFile(pathToFile, pathToCopiedFile, clientSocket);
                } else if (content.find("NO ") == 0) {
                    std::string filename = content.substr(3);
                    pathToFile = serverFolderPath + "/" + filename;
                    std::filesystem::remove(pathToFile);
                } else if (content.find("SEND ") == 0) {
                    std::string filename = content.substr(5);
                    pathToFile = clientFolderPath + "/" + filename;
                    pathToCopiedFile = serverFolderPath + "/" + filename;
                    FileManager::copyFile(pathToFile, pathToCopiedFile, clientSocket);
                    directoryForCopy = pathToCopiedFile;
                    ChatMessage message{content, clientName, filename, clientSocket, room->nextMessageId++};
                    room->addMessageToQueue(message);
                } else {
                    ChatMessage message{content, clientName, " ", clientSocket, room->nextMessageId++};
                    room->addMessageToQueue(message);
                }
            } else {
                serverSocket.reportError("Received failed.");
                break;
            }
        }

        room->removeClient(clientSocket);
        close(clientSocket);
    }
};

int main() {
    ChatServer newChatServer;
    return 0;
}
