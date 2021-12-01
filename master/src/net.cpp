#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "net.h"

#define BUFFER_SIZE 1024

int socket_connect() {
  // These can stay hardcoded
  std::string host = "127.0.0.1";
  int port = 80;

  struct sockaddr_in addr;
  int on = 1;
  int sock;

  inet_pton(AF_INET, host.c_str(), &(addr.sin_addr));
  addr.sin_port = htons(port);
  addr.sin_family = AF_INET;
  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(int));

  if (sock < 0) {
    perror("socket_connect: error opening socket");
    exit(1);
  }

  if (connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) ==
      -1) {
    perror("connect");
    exit(1);
  }

  return sock;
}

std::string socket_send(std::string& request_content) {
  int sock = socket_connect();

  std::string response;
  std::string request = "GET /" + request_content + " \r\n";
  char buffer[BUFFER_SIZE] = "";
  bzero(buffer, BUFFER_SIZE);

  ssize_t bytes_written = write(sock, request.c_str(), request.size());
  if (bytes_written != (ssize_t)request.size()) {
    perror("socket_send: error writing to socket");
    exit(1);
  }

  while (read(sock, buffer, BUFFER_SIZE - 1) != 0) {
    response += buffer;
    bzero(buffer, BUFFER_SIZE);
  }

  socket_close(sock);

  return response;
}

void socket_close(int sock) {
  shutdown(sock, SHUT_RDWR);
  close(sock);
}