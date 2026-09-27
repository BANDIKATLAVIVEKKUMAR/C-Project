// chat_server.cpp
// A simple "live chat" server using plain HTTP polling (no WebSockets).
// It keeps an in-memory list of chat messages and exposes two endpoints:
//
//   GET  /messages   -> returns all messages so far, one per line
//   POST /send       -> body is a new message; it gets appended to the list
//
// Multiple browser tabs poll GET /messages every couple of seconds, so
// they all eventually see everything anyone has sent. The server is
// single-threaded and handles one request at a time -- fine here because
// each request/response is tiny and takes only milliseconds, so several
// tabs polling "at once" just take turns almost instantly.
//
// Compile:  g++ chat_server.cpp -o chat_server
// Run:      ./chat_server

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

const int PORT = 8081;
const size_t MAX_MESSAGES = 50; // keep the history from growing forever

std::vector<std::string> messages; // shared state: every message ever sent

// Trim trailing \r, \n, and spaces (POST bodies often end with a stray \r\n)
std::string trim(const std::string& s) {
    size_t end = s.find_last_not_of(" \r\n\t");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

// Splits a raw HTTP request into method, path, and headers-only text
void parseRequestLine(const std::string& request, std::string& method, std::string& path) {
    size_t methodEnd = request.find(' ');
    size_t pathEnd = request.find(' ', methodEnd + 1);
    if (methodEnd == std::string::npos || pathEnd == std::string::npos) return;
    method = request.substr(0, methodEnd);
    path = request.substr(methodEnd + 1, pathEnd - methodEnd - 1);
}

// Reads Content-Length out of the header block, or 0 if absent
long parseContentLength(const std::string& headers) {
    size_t pos = headers.find("Content-Length:");
    if (pos == std::string::npos) return 0;
    pos += strlen("Content-Length:");
    return std::stol(headers.substr(pos));
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed. Is port " << PORT << " already in use?\n";
        return 1;
    }
    listen(server_fd, 5);

    std::cout << "Chat server listening on http://localhost:" << PORT << "\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) continue;

        // 1. Read the request (headers, and maybe part of the body already)
        char buf[4096] = {0};
        long n = read(client_fd, buf, sizeof(buf) - 1);
        std::string request(buf, n > 0 ? n : 0);

        size_t headerEnd = request.find("\r\n\r\n");
        std::string headers = (headerEnd == std::string::npos) ? request : request.substr(0, headerEnd);
        std::string body = (headerEnd == std::string::npos) ? "" : request.substr(headerEnd + 4);

        // 2. If there's more body left to come, keep reading until we have it all
        //    (same idea as the receive-loop in client.cpp, just on the server side)
        long contentLength = parseContentLength(headers);
        while ((long)body.size() < contentLength) {
            char more[4096];
            long extra = read(client_fd, more, sizeof(more));
            if (extra <= 0) break;
            body.append(more, extra);
        }

        std::string method, path;
        parseRequestLine(request, method, path);

        std::string responseBody;
        std::string status = "200 OK";

        if (method == "GET" && path == "/messages") {
            for (const auto& m : messages) responseBody += m + "\n";
        } else if (method == "POST" && path == "/send") {
            std::string line = trim(body);
            if (!line.empty()) {
                messages.push_back(line);
                if (messages.size() > MAX_MESSAGES) {
                    messages.erase(messages.begin()); // drop the oldest
                }
                std::cout << "New message: " << line << "\n";
            }
            responseBody = "OK";
        } else {
            status = "404 Not Found";
            responseBody = "Not found";
        }

        std::string response =
            "HTTP/1.1 " + status + "\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " + std::to_string(responseBody.size()) + "\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "\r\n" + responseBody;

        size_t sent = 0;
        while (sent < response.size()) {
            ssize_t s = send(client_fd, response.c_str() + sent, response.size() - sent, 0);
            if (s <= 0) break;
            sent += s;
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
