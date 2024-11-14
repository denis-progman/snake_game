#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <sstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace std;

// Game settings
const int width = 20;
const int height = 10;
int x, y, fruitX, fruitY, score;
vector<pair<int, int>> tail;
int nTail = 0;
enum Direction { STOP = 0, LEFT, RIGHT, UP, DOWN };
Direction dir = STOP;
bool gameOver = false;

// Debug flag
bool debug = true;

// Setup game
void Setup() {
    gameOver = false;
    dir = STOP;
    x = width / 2;
    y = height / 2;
    fruitX = rand() % width;
    fruitY = rand() % height;
    score = 0;
    nTail = 0;
    tail.clear();
}

// Game logic
void MoveSnake() {
    if (nTail > 0) {
        tail.insert(tail.begin(), make_pair(x, y));
        if (tail.size() > nTail) tail.pop_back();
    }

    switch (dir) {
        case LEFT: x--; break;
        case RIGHT: x++; break;
        case UP: y--; break;
        case DOWN: y++; break;
        default: break;
    }

    if (x >= width) x = 0;
    else if (x < 0) x = width - 1;
    if (y >= height) y = 0;
    else if (y < 0) y = height - 1;

    for (int i = 0; i < nTail; i++) {
        if (tail[i].first == x && tail[i].second == y) gameOver = true;
    }

    if (x == fruitX && y == fruitY) {
        score += 10;
        nTail++;
        fruitX = rand() % width;
        fruitY = rand() % height;
    }
}

// Generate JSON representation of the game state
string GetGameState() {
    ostringstream oss;
    oss << "{ \"gameOver\": " << (gameOver ? "true" : "false")
        << ", \"score\": " << score
        << ", \"snake\": [";
    for (size_t i = 0; i < tail.size(); ++i) {
        oss << "{ \"x\": " << tail[i].first << ", \"y\": " << tail[i].second << "}";
        if (i != tail.size() - 1) oss << ", ";
    }
    oss << "], \"fruit\": { \"x\": " << fruitX << ", \"y\": " << fruitY << " } }";
    return oss.str();
}

// HTTP Response Helper with CORS and optional debug logging
string HTTPResponse(const string& body, const string& contentType = "application/json") {
    ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"  // Allow CORS
        << "Content-Length: " << body.size() << "\r\n\r\n"
        << body;

    // Print the response body if debug mode is enabled
    if (debug) {
        cout << "API Response Body:\n" << body << endl;
    }

    return oss.str();
}

// Extracts the HTTP method and path from the request
pair<string, string> ParseHTTPRequest(const string& request) {
    istringstream reqStream(request);
    string method, path;
    reqStream >> method >> path;  // Get the HTTP method and path
    if (debug) {
        cout << "HTTP Method: " << method << ", Path: " << path << endl;
    }
    return make_pair(method, path);
}

// Parse the direction from the URL path
int ParseDirection(const string& path) {
    size_t pos = path.find("/game/direction/");
    if (pos != string::npos) {
        pos += strlen("/game/direction/");
        return stoi(path.substr(pos));
    }
    return -1;  // Return -1 if parsing fails
}

// Basic HTTP Server to handle GET /game/state and POST /game/direction/<int>
void RunServer() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");  // Bind to localhost
    address.sin_port = htons(8080);

    // Bind
    if (::bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (true) {
        cout << "Waiting for connections..." << endl;
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        // Read request
        char buffer[30000] = {0};
        read(new_socket, buffer, 30000);
        string request(buffer);

        // Parse HTTP request method and path
        auto [method, path] = ParseHTTPRequest(request);

        if (method == "GET" && path == "/game/state") {
            string response = HTTPResponse(GetGameState());
            send(new_socket, response.c_str(), response.size(), 0);
        }
        else if (method == "POST" && path.find("/game/direction/") == 0) {
            int newDir = ParseDirection(path);
            if (newDir >= 1 && newDir <= 4) {
                dir = static_cast<Direction>(newDir);
            }
            string response = HTTPResponse("{ \"status\": \"OK\" }");
            send(new_socket, response.c_str(), response.size(), 0);
        } else {
            string response = HTTPResponse("{ \"status\": \"Not Found\" }", "text/plain");
            send(new_socket, response.c_str(), response.size(), 0);
        }

        close(new_socket);
    }
}

int main() {
    srand(time(0));
    Setup();

    // Start the game logic in a separate thread
    thread gameThread([]() {
        while (!gameOver) {
            MoveSnake();
            this_thread::sleep_for(chrono::milliseconds(200));
        }
    });

    // Start the server
    RunServer();

    gameThread.join();
    return 0;
}
