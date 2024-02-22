#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <mutex>

using namespace std;

std::mutex io_mutex;

class Client {
private:
    int port = 12341;
    const char* serverIp = "127.0.0.1";
    int clientSocket;
    struct sockaddr_in serverAddr;
    std::thread receiveThread;

public:
    Client() {
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == -1) {
            perror("Error creating socket");
        } else {
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            inet_pton(AF_INET, serverIp, &(serverAddr.sin_addr));

            if (connect(clientSocket, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1) {
                perror("Connect failed");
                close(clientSocket);
            }
        }
    }

    void printWelcomeMessage() const {
        cout << "\033[1;32m";
        cout << "Welcome to Chat Application!" << endl;
        cout << "You can start chatting after entering your username and choosing the room." << endl;
        cout << "Type 'EXIT' to quit the chat." << endl << endl;
        cout << "\033[0m";
    }

    void receiveServerMessage() {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            std::lock_guard<std::mutex> lock(io_mutex);
            std::string message(buffer, bytesReceived);

            if (message.find("receive") != std::string::npos) {
                size_t startPos = message.find("wants to send ") + std::string("wants to send ").length();
                size_t endPos = message.find(" file");
                std::string filename = message.substr(startPos, endPos - startPos);
                message += "\nResponse (YES/NO and filename): ";

                cout << "\033[1;33m";
                cout << message;
                cout << "\033[0m";

                std::string response;
                std::getline(std::cin, response);
                send(clientSocket, response.c_str(), response.length(), 0);
            } else {
                cout << "\033[1;36m";
                cout << message << endl;
                cout << "\033[0m";
            }
        } else if (bytesReceived == 0) {
            std::cerr << "Connection closed by server." << std::endl;
        } else {
            std::cerr << "Failed to receive message from server." << std::endl;
        }
    }

    void sendClientName() const {
        string clientName;
        cout << "\033[1;35m";
        cout << "Please enter your username: ";
        cout << "\033[0m";
        getline(cin, clientName);
        send(clientSocket, clientName.c_str(), clientName.size(), 0);
    }

    void chooseRoom() const {
        string roomName;
        cout << "\033[1;35m";
        cout << "Enter room name: ";
        cout << "\033[0m";
        getline(cin, roomName);
        send(clientSocket, roomName.c_str(), roomName.size(), 0);
    }




    void chat() {
        string message;
        while (true) {
            if (receiveThread.joinable()) {
                receiveThread.join();
            }
            receiveThread = std::thread(&Client::receiveServerMessage, this);
            receiveThread.detach();

            cout << "Enter the message: ";
            getline(std::cin, message);
            if (message.find("REJOIN") == 0) {
                send(clientSocket, message.c_str(), message.size(), 0);
                cout << "\nYou can rejoin to another room." << endl;
                sendClientName();
                chooseRoom();
                continue;
            }
            if (message == "EXIT") {
                if (receiveThread.joinable()) {
                    receiveThread.join();
                }
                break;
            }
            send(clientSocket, message.c_str(), message.size(), 0);
        }
    }

    ~Client() {
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
        close(clientSocket);
    }

};

int main() {
    Client client;
    client.printWelcomeMessage();
    client.sendClientName();
    client.chooseRoom();
    client.chat();
    return 0;
}
