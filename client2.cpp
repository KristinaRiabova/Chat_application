#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fstream>

void* receiveMessages(void* arg);

const char* clientFolderPath = "client2_files";

void sendFile(int clientSocket, const std::string& fileName) {
    std::ifstream fileToSend(std::string(clientFolderPath) + "/" + fileName, std::ios::binary);
    if (!fileToSend) {
        std::cerr << "Failed to open file: " << fileName << std::endl;
        return;
    }

    // Определяем размер файла
    fileToSend.seekg(0, std::ios::end);
    int fileSize = fileToSend.tellg();
    fileToSend.seekg(0, std::ios::beg);

    // Отправляем размер файла
    send(clientSocket, &fileSize, sizeof(fileSize), 0);

    // Создаем буфер для файла
    char buffer[fileSize];
    // Читаем содержимое файла в буфер
    fileToSend.read(buffer, fileSize);
    fileToSend.close();

    // Отправляем содержимое файла
    send(clientSocket, buffer, fileSize, 0);
}

void saveFile(const std::string& fileName, char* fileData, size_t fileSize) {
    std::ofstream outfile(fileName, std::ios::binary);
    if (outfile.is_open()) {
        outfile.write(fileData, fileSize);
        std::cout << "File saved: " << fileName << std::endl;
    } else {
        std::cerr << "Error creating file: " << fileName << std::endl;
    }
}

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(8080);

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Connection failed.\n";
        close(clientSocket);
        return 1;
    }

    std::cout << "Connected to server.\n";

    int roomID;
    std::cout << "Enter the room ID: ";
    std::cin >> roomID;
    std::cin.ignore();

    send(clientSocket, &roomID, sizeof(roomID), 0);

    pthread_t receiveThread;
    if (pthread_create(&receiveThread, nullptr, receiveMessages, &clientSocket) != 0) {
        std::cerr << "Failed to create receive thread.\n";
        close(clientSocket);
        return 1;
    }

    char message[4096];
    while (true) {
        std::cout << "You: ";
        std::cin.getline(message, sizeof(message));
        if (strncmp(message, "/join ", 6) == 0 || strncmp(message, "/file ", 6) == 0) {
            send(clientSocket, message, strlen(message), 0);
            if (strncmp(message, "/file", 5) == 0) {
                send(clientSocket, message, strlen(message), 0);
                std::string fileName = message + 6;

                std::ifstream fileToSend(std::string(clientFolderPath) + "/" + fileName, std::ios::binary);
                if (!fileToSend) {
                    std::cerr << "Failed to open file: " << fileName << std::endl;
                    continue;
                }

                fileToSend.seekg(0, std::ios::end);
                int fileSize = fileToSend.tellg();
                fileToSend.seekg(0, std::ios::beg);

                send(clientSocket, &fileSize, sizeof(fileSize), 0);

                char buffer[fileSize];
                fileToSend.read(buffer, fileSize);
                fileToSend.close();

                send(clientSocket, buffer, fileSize, 0);
                continue;
            }
        }
        send(clientSocket, message, strlen(message), 0);
    }

    close(clientSocket);
    return 0;
}

void* receiveMessages(void* arg) {
    int clientSocket = *((int*)arg);
    char buffer[4096];

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::cerr << "Server disconnected.\n";
            break;
        }
        buffer[bytesReceived] = '\0';
        std::cout << "Server: " << buffer << std::endl;
    }

    close(clientSocket);
    pthread_exit(nullptr);
}
