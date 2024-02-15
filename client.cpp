#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

void* receiveMessages(void* arg);

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

    // Sending room ID to the server
    int roomID;
    std::cout << "Enter the room ID: ";
    std::cin >> roomID;
    std::cin.ignore(); // Clear input buffer
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
        if (strncmp(message, "/join ", 6) == 0) {
            // Если введена команда перехода в другую комнату
            send(clientSocket, message, strlen(message), 0);
            continue; // Продолжаем вводить сообщения после отправки команды
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