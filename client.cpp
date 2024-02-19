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

    void receiveServerMessage() {
        char fileMessage[1024];
        memset(fileMessage, 0, sizeof(fileMessage));
        ssize_t bytes = recv(clientSocket, fileMessage, sizeof(fileMessage), 0);
        if (bytes > 0) {
            lock_guard<std::mutex> io_lock(io_mutex);
            string messageToStr(fileMessage, bytes);
            if (messageToStr.find("receive") != std::string::npos) {
                size_t startPos = messageToStr.find("wants to send ") + std::string("wants to send ").length();;
                size_t endPos = messageToStr.find(" file");
                string filename = messageToStr.substr(startPos, endPos - startPos);
                messageToStr += "\nResponse (YES/NO and filename): ";

                cout << messageToStr;
                string response;
                getline(cin, response);
                send(clientSocket, response.c_str(), response.length(), 0);
            } else {
                cout << messageToStr << endl;
            }
        } else {
            cerr << "Failed to receive message from server." << endl;
        }
    }

    void sendClientName() const {
        string clientName;
        cout << "Please enter your username: ";
        getline(cin, clientName);
        send(clientSocket, clientName.c_str(), clientName.size(), 0);
    }

    void chooseRoom() const {
        string roomName;
        cout << "Enter room name: ";
        getline(cin, roomName);
        send(clientSocket, roomName.c_str(), roomName.size(), 0);
    };

    void chat() {
        string message;
        while (true) {
            cout << "Enter the message: ";
            std::getline(std::cin, message);
            if (message == "EXIT") {
                break;
            }
            send(clientSocket, message.c_str(), message.size(), 0);
            receiveServerMessage();
        }
    }

    ~Client() {
        close(clientSocket);
    }
};

int main() {
    Client client;
    client.sendClientName();
    client.chooseRoom();
    client.chat();
    return 0;
}
