##General System Description

Client-server chat application developed in C++ using sockets for message exchange between clients and the server.
Port: System utilizes port 12341 for message exchange between clients and the server.
IP Address: For local testing of application, I use the standard IP address "127.0.0.1"
Message Buffers: In system, a standard buffer size of 1024 bytes is used for exchanging textual messages between clients and the server. This buffer size provides sample space for most text messages users may send and receive in the chat.
Files and Large Messages: To transmit files between clients and the server, a special procedure is employed to handle larger file sizes. When transmitting files, the buffer size may be dynamically adapted based on the size of the file being transferred to ensure efficient data transmission.

##Application protocol description
TCP Connection Handling: The provided code effectively manages TCP connections, with clients initiating connections and the server accepting them, allocating a new thread for each client.
Room Management: The implementation maintains chat rooms efficiently, allowing clients to join, send messages, and share files within specific rooms.
Mutexes and Threads: The use of mutexes and threads ensures proper synchronization and prevents data corruption in a multi-threaded environment.
File Sharing: The file sharing functionality is implemented, enabling clients to share files and others in the room to accept or decline them.
Binary Data Transfer: Binary data transfer is employed for efficient transmission of messages and files between clients and the server.
Command Exchange: Clients communicate with the server using predefined commands (e.g., SEND, EXIT) and exchange messages with other clients. The server processes these commands and messages accordingly, ensuring seamless interaction within the chat environment.
Threads: The use of threads allows for efficient concurrency management within the server application. Each client connection and chat room is handled in a separate thread, enabling parallel processing of client requests and facilitating real-time communication among users. This architecture enhances the scalability and responsiveness of the chat system, accommodating a growing number of users and ensuring optimal performance.


