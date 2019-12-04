#include <stdio.h> // for perror
#include <string.h> // for memset
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket
#include <pthread.h>
#include <vector>
#include <string>
using namespace std;

struct Client {
	pthread_t th;
	int childfd;
	Client(pthread_t th, int childfd) {
		this->th = th;
		this->childfd = childfd;
	}
};

pthread_mutex_t mutex_lock;
bool broadcast_mode = false;
vector<Client> clientInfo;

void* t_func(void *data) {
	printf("connected\n");
	
	int childfd = *((int *)data);
	
	while (true) {
		const static int BUFSIZE = 1024;
		char buf[BUFSIZE];
		
		ssize_t received = recv(childfd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			break;
		}
		buf[received] = '\0';
		printf("%s\n", buf);
		
		ssize_t sent = send(childfd, buf, strlen(buf), 0);
		if(sent == 0) {
			perror("send failed");
			break;
		}
		if(broadcast_mode) {
			pthread_mutex_lock(&mutex_lock);
			for(int i = 0; i < clientInfo.size(); i++) {
				if(clientInfo[i].childfd == childfd) continue;
				ssize_t sent = send(clientInfo[i].childfd, buf, strlen(buf), 0);
				if(sent == 0) {
					perror("send failed");
					clientInfo.erase(clientInfo.begin() + i--);
					pthread_cancel(clientInfo[i].th);
					printf("disconnected\n");
				}
			}
			pthread_mutex_unlock(&mutex_lock);
		}
	}
	
	pthread_mutex_lock(&mutex_lock);
	for(int i = 0; i < clientInfo.size(); i++) {
		if(clientInfo[i].childfd == childfd) {
			clientInfo.erase(clientInfo.begin() + i);
			break;
		}
	}
	pthread_mutex_unlock(&mutex_lock);
	
	printf("disconnected\n");
}

void usage() {
	printf("syntax : echo_server <port> [-b]\n");
	printf("sample : echo_server 1234 -b\n");
}

void clean() {
	pthread_mutex_lock(&mutex_lock);
	for(int i = 0; i < clientInfo.size(); i++) {
		pthread_cancel(clientInfo[i].th);
	}
	pthread_mutex_unlock(&mutex_lock);
}

int main(int argc, char **argv) {
	if(argc == 3 && strcmp(argv[2], "-b") == 0) {
		broadcast_mode = true;
	}
	else if(argc != 2) {
		usage();
		return 1;
	}
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(stoi(argv[1]));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2);
	if (res == -1) {
		perror("listen failed");
		return -1;
	}
	
	pthread_mutex_init(&mutex_lock, NULL);
	
	while (true) {
		struct sockaddr_in addr;
		socklen_t clientlen = sizeof(sockaddr);
		int childfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
		if (childfd < 0) {
			perror("ERROR on accept");
			break;
		}
		
		pthread_t th;
		if(pthread_create(&th, NULL, t_func, (void *)&childfd) < 0)
	    {
	        perror("thread create error : ");
	        clean();
	        return -1;
	    }
		pthread_detach(th);
		pthread_mutex_lock(&mutex_lock);
		clientInfo.push_back(Client(th, childfd));
		pthread_mutex_unlock(&mutex_lock);
	}

	clean();
	close(sockfd);
}
