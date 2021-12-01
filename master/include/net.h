#ifndef _NET_H
#define _NET_H
#include <string>

int socket_connect();
std::string socket_send(std::string& request_content);
void socket_close(int sock);

#endif  // _NET_H
