#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <map>
#include <vector>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <set>
#include <cstdlib>
#include <ctime>
#include "protocol.hpp"

#define PORT 12345

struct Client {
    int fd;
    std::string nick;
    std::vector<char> incomingBuffer;
    int currentRoomId = -1;
    int score = 0; 
};

struct Room {
    int id;
    std::string name;
    int hostFd;
    std::vector<int> players;
    bool gameStarted = false;
    
    std::map<int, std::string> playerAnswers;
    std::map<int, std::string> playerVotes;
    
    int currentRound = 0;
    int maxRounds = 3;
};

class GameServer {
private:
    int serverPort;
    int serverSock;
    std::vector<struct pollfd> poll_fds;
    std::map<int, Client> clients;
    std::map<int, Room> rooms;
    int nextRoomId = 1;
    std::map<int, time_t> answerTimeouts;
    std::map<int, time_t> lastTimeUpdate;
    std::map<int, time_t> nextRoundStartTimes;

    void setNonBlocking(int sock) {
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    void sendToClient(int fd, MsgType type, const std::string& data) {
        auto msg = createMessage(type, data);
        send(fd, msg.data(), msg.size(), 0);
    }

    void broadcastToRoom(int roomId, MsgType type, const std::string& data) {
        if (rooms.find(roomId) == rooms.end()) return;
        
        const auto& room = rooms[roomId];
        for (int playerFd : room.players) {
            sendToClient(playerFd, type, data);
        }
    }

    std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    char getRandomLetter() {
        return 'A' + (rand() % 26);
    }

    void calculateScores(int roomId) {
        Room& room = rooms[roomId];

        std::map<int, std::map<std::string, int>> vetos;

        for (auto const& [pid, voteStr] : room.playerVotes) {
            auto parts = split(voteStr, ';');
            for (const auto& part : parts) {
                auto kv = split(part, ':');
                if (kv.size() == 2) {
                    try {
                        int catIdx = std::stoi(kv[0]);
                        std::string word = kv[1];
                        vetos[catIdx][word]++;
                    } catch (...) {}
                }
            }
        }

        std::map<int, std::map<std::string, int>> validAnswersCounts;

        for (auto const& [pid, ansStr] : room.playerAnswers) {
            auto parts = split(ansStr, ';');
            for (size_t i = 0; i < parts.size() && i < 5; ++i) {
                std::string word = parts[i];
                if (word.empty()) continue;

                int votesAgainst = vetos[i][word];
                int totalPlayers = room.players.size();

                bool accepted = true;
                if (totalPlayers <= 1) accepted = true;
                else accepted = (votesAgainst * 2 < totalPlayers);

                if (accepted) {
                    validAnswersCounts[i][word]++;
                }
            }
        }

        std::string roundSummary = "";
        std::string totalSummary = "";

        for (int pid : room.players) {
            std::string ansStr = "";
            if (room.playerAnswers.find(pid) != room.playerAnswers.end()) ansStr = room.playerAnswers[pid];
            auto parts = split(ansStr, ';');
            int roundPoints = 0;

            for (size_t i = 0; i < parts.size() && i < 5; ++i) {
                std::string word = parts[i];
                if (word.empty()) continue;

                int votesAgainst = vetos[i][word];
                int totalPlayers = room.players.size();

                bool accepted = true;
                if (totalPlayers <= 1) accepted = true;
                else accepted = (votesAgainst * 2 < totalPlayers);

                if (accepted) {
                    if (validAnswersCounts[i][word] == 1) {
                        roundPoints += 10;
                    } else {
                        roundPoints += 5;
                    }
                }
            }

            roundSummary += clients[pid].nick + ":" + std::to_string(roundPoints) + ";";

            clients[pid].score += roundPoints;
            totalSummary += clients[pid].nick + ":" + std::to_string(clients[pid].score) + ";";
        }

        broadcastToRoom(roomId, MsgType::ROUND_END, roundSummary);

        room.playerAnswers.clear();
        room.playerVotes.clear();

        if (room.currentRound < room.maxRounds) {
            nextRoundStartTimes[roomId] = time(NULL) + 5;
        } else {
            broadcastToRoom(roomId, MsgType::GAME_END, totalSummary);
            for (int pid : room.players) {
                clients[pid].currentRoomId = -1;
            }
            rooms.erase(roomId);
            answerTimeouts.erase(roomId);
            lastTimeUpdate.erase(roomId);
            nextRoundStartTimes.erase(roomId);
        }
    }

    void processMessage(Client& client, MsgHeader header, const std::vector<char>& body) {
        std::string dataStr(body.begin(), body.end());

        switch (header.type) {
            case MsgType::LOGIN: {
                bool nickTaken = false;
                for (const auto& pair : clients) {
                    if (pair.second.nick == dataStr) {
                        nickTaken = true;
                        break;
                    }
                }

                if (nickTaken) {
                    sendToClient(client.fd, MsgType::LOGIN_FAIL, "Nick jest zajety!");
                } else {
                    client.nick = dataStr;
                    sendToClient(client.fd, MsgType::LOGIN_OK, "Witaj w lobby!");
                }
                break;
            }

            case MsgType::CREATE_ROOM: {
                if (client.nick.empty()) return; 
                std::string roomName = dataStr;
                
                bool nameTaken = false;
                for (const auto& [id, room] : rooms) {
                    if (room.name == roomName) {
                        nameTaken = true;
                        break;
                    }
                }
                
                if (nameTaken) {
                    sendToClient(client.fd, MsgType::CREATE_ROOM_FAIL, "Nazwa pokoju jest zajeta!");
                    break;
                }
                int newId = nextRoomId++;
                
                Room newRoom;
                newRoom.id = newId;
                newRoom.name = roomName;
                newRoom.hostFd = client.fd;
                newRoom.players.push_back(client.fd);
                
                rooms[newId] = newRoom;
                client.currentRoomId = newId;
                
                sendToClient(client.fd, MsgType::CREATE_ROOM_OK, roomName);
                break;
            }

            case MsgType::GET_ROOM_LIST: {
                std::string list;
                for (const auto& [id, room] : rooms) {
                    std::string state = room.gameStarted ? "inprogress" : "waiting";
                    list += std::to_string(id) + ":" + room.name + ":" + std::to_string(room.players.size()) + ":" + state + ";";
                }
                sendToClient(client.fd, MsgType::ROOM_LIST, list);
                break;
            }

            case MsgType::JOIN_ROOM: {
                std::string roomName = dataStr;
                
                bool found = false;
                int roomId = -1;
                for (const auto& [id, room] : rooms) {
                    if (room.name == roomName && !room.gameStarted) {
                        roomId = id;
                        found = true;
                        break;
                    }
                }
                
                if (found) {
                    rooms[roomId].players.push_back(client.fd);
                    client.currentRoomId = roomId;
                    
                    std::string playerListStr = "";
                    for (int pid : rooms[roomId].players) {
                        if (!playerListStr.empty()) playerListStr += ",";
                        playerListStr += clients[pid].nick;
                    }

                    sendToClient(client.fd, MsgType::JOIN_ROOM_OK, rooms[roomId].name + ";" + playerListStr);
                    
                    for (int pid : rooms[roomId].players) {
                        if (pid != client.fd) {
                            sendToClient(pid, MsgType::NEW_PLAYER_JOINED, client.nick);
                        }
                    }
                } else {
                    sendToClient(client.fd, MsgType::JOIN_ROOM_FAIL, "Brak pokoju o takiej nazwie");
                }
                break;
            }

            case MsgType::START_GAME: {
                int roomId = client.currentRoomId;
                if (roomId == -1 || rooms.find(roomId) == rooms.end()) return;

                Room& room = rooms[roomId];

                if (room.hostFd != client.fd) {
                    sendToClient(client.fd, MsgType::GAME_START_FAIL, "Nie jestes hostem!");
                    break;
                }

                if (room.players.size() < 2) { 
                    sendToClient(client.fd, MsgType::GAME_START_FAIL, "Za malo graczy!");
                    break;
                }

                room.gameStarted = true;
                room.currentRound = 1;
                for (int pid : room.players) {
                    clients[pid].score = 0;
                }
                room.playerAnswers.clear();
                room.playerVotes.clear();
                char letter = getRandomLetter();
                std::string gameData = std::string(1, letter) + ";" + std::to_string(room.currentRound) + ";" + std::to_string(room.maxRounds);
                broadcastToRoom(roomId, MsgType::GAME_STARTED, gameData);
                answerTimeouts[roomId] = time(NULL) + 30;
                lastTimeUpdate[roomId] = time(NULL);
                break;
            }
            
            case MsgType::SUBMIT_ANSWERS: {
                int roomId = client.currentRoomId;
                if (roomId == -1) return;
                
                Room& room = rooms[roomId];
                room.playerAnswers[client.fd] = dataStr;
                
                if (room.playerAnswers.size() == room.players.size()) {
                    std::set<std::string> cats[5];
                    
                    for (auto const& [pid, ansStr] : room.playerAnswers) {
                        auto parts = split(ansStr, ';');
                        for (size_t i=0; i<parts.size() && i<5; ++i) {
                            if (!parts[i].empty()) {
                                cats[i].insert(parts[i]);
                            }
                        }
                    }
                    
                    std::string payload = "";
                    std::string labels[] = {"Panstwo", "Miasto", "Zwierze", "Roslina", "Rzecz"};
                    
                    for (int i=0; i<5; ++i) {
                        payload += labels[i] + ":";
                        bool first = true;
                        for (const auto& ans : cats[i]) {
                            if (!first) payload += ",";
                            payload += ans;
                            first = false;
                        }
                        payload += ";";
                    }
                    
                    broadcastToRoom(roomId, MsgType::VERIFICATION_START, payload);
                }
                break;
            }

            case MsgType::SEND_VOTE: {
                int roomId = client.currentRoomId;
                if (roomId == -1) return;
                
                Room& room = rooms[roomId];
                room.playerVotes[client.fd] = dataStr;
                
                if (room.playerVotes.size() == room.players.size()) {
                    calculateScores(roomId);
                }
                break;
            }

            case MsgType::LEAVE_ROOM: {
                int roomId = client.currentRoomId;
                if (roomId != -1 && rooms.find(roomId) != rooms.end()) {
                    Room& room = rooms[roomId];
                    bool wasHost = (client.fd == room.hostFd);
                    room.players.erase(std::remove(room.players.begin(), room.players.end(), client.fd), room.players.end());
                    room.playerAnswers.erase(client.fd);
                    room.playerVotes.erase(client.fd);
                    client.currentRoomId = -1;
                    if (wasHost) {
                        for (int pid : room.players) {
                            clients[pid].currentRoomId = -1;
                            sendToClient(pid, MsgType::HOST_LEFT, "");
                        }
                        rooms.erase(roomId);
                    } else {
                        for (int pid : room.players) {
                            sendToClient(pid, MsgType::PLAYER_LEFT, client.nick);
                        }
                        if (room.players.empty()) {
                            rooms.erase(roomId);
                        }
                    }
                }
                break;
            }

            default:
                std::cout << "Nieznany typ" << std::endl;
        }
    }

    void handleDisconnect(int fd) {
        std::cout << "Klient " << fd << " rozlaczyl sie." << std::endl;
        
        int roomId = clients[fd].currentRoomId;
        if (roomId != -1 && rooms.count(roomId)) {
            Room& room = rooms[roomId];
            bool wasHost = (fd == room.hostFd);
            auto& players = room.players;
            players.erase(std::remove(players.begin(), players.end(), fd), players.end());
            
            room.playerAnswers.erase(fd);
            room.playerVotes.erase(fd);

            if (wasHost) {
                for (int pid : players) {
                    clients[pid].currentRoomId = -1;
                    sendToClient(pid, MsgType::HOST_LEFT, "");
                }
                rooms.erase(roomId);
            } else {
                for (int pid : players) {
                    sendToClient(pid, MsgType::PLAYER_LEFT, clients[fd].nick);
                }
                if (players.empty()) {
                    rooms.erase(roomId);
                }
            }
        }

        close(fd);
        clients.erase(fd);
        
        auto it = std::remove_if(poll_fds.begin(), poll_fds.end(), 
                                 [fd](const struct pollfd& p) { return p.fd == fd; });
        poll_fds.erase(it, poll_fds.end());
    }

    void handleInput(int fd) {
        Client& client = clients[fd];
        char tempBuff[1024];
        ssize_t bytesRead = read(fd, tempBuff, sizeof(tempBuff));

        if (bytesRead <= 0) {
            handleDisconnect(fd);
            return;
        }

        client.incomingBuffer.insert(client.incomingBuffer.end(), tempBuff, tempBuff + bytesRead);

        while (true) {
            if (client.incomingBuffer.size() < sizeof(MsgHeader)) break;
            MsgHeader* header = reinterpret_cast<MsgHeader*>(client.incomingBuffer.data());
            uint32_t dataLen = ntohl(header->len);
            if (client.incomingBuffer.size() < sizeof(MsgHeader) + dataLen) break;

            std::vector<char> body(
                client.incomingBuffer.begin() + sizeof(MsgHeader),
                client.incomingBuffer.begin() + sizeof(MsgHeader) + dataLen
            );
            MsgHeader currentHeader = *header;
            client.incomingBuffer.erase(
                client.incomingBuffer.begin(),
                client.incomingBuffer.begin() + sizeof(MsgHeader) + dataLen
            );
            processMessage(client, currentHeader, body);
        }
    }

public:
    GameServer(int port = PORT) : serverPort(port) {
        srand(time(NULL));
        serverSock = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSock < 0) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            exit(1);
        }
        int opt = 1;
        setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(serverPort);
        addr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(serverSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to bind to port " << serverPort << ": " << strerror(errno) << std::endl;
            close(serverSock);
            exit(1);
        }
        if (listen(serverSock, 10) < 0) {
            std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
            close(serverSock);
            exit(1);
        }
        setNonBlocking(serverSock);
        
        poll_fds.push_back({serverSock, POLLIN, 0});
    }

    void run() {
        std::cout << "Serwer nasluchuje na porcie " << serverPort << std::endl;
        while (true) {
            int ret = poll(poll_fds.data(), poll_fds.size(), 1000);
            if (ret < 0) break;

            if (poll_fds[0].revents & POLLIN) {
                int newFd = accept(serverSock, nullptr, nullptr);
                if (newFd >= 0) {
                    setNonBlocking(newFd);
                    clients[newFd] = Client{newFd, "", {}, -1};
                    poll_fds.push_back({newFd, POLLIN, 0});
                    std::cout << "Nowe polaczenie: " << newFd << std::endl;
                }
            }

            for (size_t i = 1; i < poll_fds.size(); ++i) {
                if (poll_fds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
                    handleInput(poll_fds[i].fd);
                }
            }

            time_t now = time(NULL);
            for (auto it = answerTimeouts.begin(); it != answerTimeouts.end(); ) {
                int roomId = it->first;
                if (rooms.find(roomId) != rooms.end() && rooms[roomId].gameStarted && now > it->second) {
                    broadcastToRoom(roomId, MsgType::TIME_UP, "");
                    it = answerTimeouts.erase(it);
                } else {
                    ++it;
                }
            }

            for (auto& [roomId, timeout] : answerTimeouts) {
                if (rooms.find(roomId) != rooms.end() && rooms[roomId].gameStarted) {
                    time_t remaining = timeout - now;
                    if (remaining > 0 && now - lastTimeUpdate[roomId] >= 1) {
                        broadcastToRoom(roomId, MsgType::TIME_LEFT, std::to_string(remaining));
                        lastTimeUpdate[roomId] = now;
                    }
                }
            }

            for (auto it = nextRoundStartTimes.begin(); it != nextRoundStartTimes.end(); ) {
                int roomId = it->first;
                time_t startAt = it->second;
                if (rooms.find(roomId) == rooms.end()) {
                    it = nextRoundStartTimes.erase(it);
                    continue;
                }

                Room& room = rooms[roomId];
                if (now >= startAt) {
                    room.currentRound++;
                    char letter = getRandomLetter();
                    std::string gameData = std::string(1, letter) + ";" + std::to_string(room.currentRound) + ";" + std::to_string(room.maxRounds);
                    broadcastToRoom(roomId, MsgType::GAME_STARTED, gameData);
                    answerTimeouts[roomId] = time(NULL) + 30;
                    lastTimeUpdate[roomId] = time(NULL);
                    it = nextRoundStartTimes.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    
    ~GameServer() {
        close(serverSock);
    }
};

int main(int argc, char** argv) {
    int port = PORT;
    if (argc >= 2) {
        try {
            port = std::stoi(argv[1]);
            if (port <= 0 || port > 65535) port = PORT;
        } catch (...) {
            port = PORT;
        }
    }

    GameServer server(port);
    server.run();
    return 0;
}
