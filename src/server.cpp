#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <map>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <sstream>
#include <thread>

struct HTTPRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
};
struct HTTPResponse {
    std::string status;
    std::string content_type;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string to_string() {
        std::string response;
        response += status + "\r\n";
        response += "Content-Type: " + content_type + "\r\n";
        for (const auto& header : headers) {
            response += header.first + ": " + header.second + "\r\n";
        }
        response += "\r\n";
        response += body;
        return response;
    }
};
void write_response(int client_fd, HTTPResponse response) {
    std::string response_str = response.to_string();
    ssize_t bytes_written = write(client_fd, response_str.c_str(), response_str.size());
    if (bytes_written < 0) {
        std::cerr << "Failed to write response to client\n";
        close(client_fd);
    }
}
HTTPRequest parse_request(std::string request) {
    HTTPRequest req;
    std::stringstream ss(request);
    std::string line;
    std::getline(ss, line);
    std::istringstream line_ss(line);
    line_ss >> req.method >> req.path >> req.version;
    while (std::getline(ss, line) && !line.empty()) {
        size_t pos = line.find(":");
        if (pos != std::string::npos) {
            std::string header_name = line.substr(0, pos);
            std::string header_value = line.substr(pos + 2);
            header_value.erase(header_value.end() - 1);
            req.headers[header_name] = header_value;
        }
    }
    std::stringstream body_ss;
    std::getline(ss, line);
    body_ss << line;
    while (std::getline(ss, line)) {
        body_ss << "\n" << line;
    }
    req.body = body_ss.str();
    return req;
}
void handle_http(int client, struct sockaddr_in client_addr){

  char buffer[1024];
  int ret = read(client, buffer, sizeof(buffer));
  // std::cout<<buffer<<std::endl;
  std::string request(buffer);
  auto a = parse_request(request);
  // std::cout<<a.method<<std::endl;
  // std::cout<<a.path<<std::endl;
  // std::cout<<a.version<<std::endl;
  // std::cout<<a.headers["Host"]<<std::endl;
  // std::cout<<a.headers["User-Agent"]<<std::endl;
  // std::cout<<a.headers["Accept"]<<std::endl;
  if(a.method == "GET"){
    if(a.path == "/"){
      HTTPResponse response = { "HTTP/1.1 200 OK", "text/plain", {}, "Hello, World!" };
      write_response(client, response);
    }else if(a.path == "/user-agent"){
      HTTPResponse response = { "HTTP/1.1 200 OK", "text/plain", { {"Content-Length", std::to_string(a.headers["User-Agent"].length())} },a.headers["User-Agent"]};
      write_response(client , response);
    } else if (a.path.substr(0, 6) == "/echo/") {
      std::string subStr = a.path.substr(6);
      HTTPResponse response = { "HTTP/1.1 200 OK", "text/plain", { {"Content-Length", std::to_string(subStr.length())} }, subStr };
      write_response(client, response);
    }else {
      HTTPResponse response = { "HTTP/1.1 404 Not Found", "text/plain", {}, "Not Found" };
      write_response(client, response);
    }
  }else{
    HTTPResponse response = { "HTTP/1.1 405 Method Not Allowed", "text/plain", {}, "Method Not Allowed" };
    write_response(client, response);
  }
}
void signal_callback_handler(int signum) {
   std::cout << "Caught signal " << signum << std::endl;
   // Terminate program
   exit(signum);
}
int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  int client_fd;
  while (1)
  {
      client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
      std::cout << "Client connected\n";
      std::thread th(handle_http, client_fd, client_addr);
      th.detach();
  };


  close(server_fd);
  return 0;
}
