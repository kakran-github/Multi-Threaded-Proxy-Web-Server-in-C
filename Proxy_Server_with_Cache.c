#include "Parse_Proxy.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

#define MAX_CLIENTS 10
typedef struct cache_element cache_element;
//Reduncing redundancy of declaring struct again and again

struct cache_element{
    char* data; //storing data
    int len; //data length
    char* url; //finding request using url
    time_t lru_time_track;
    cache_element* next;
};

// Declaring functions

cache_element* find(char* url); // to iterate linked list
// to add and remove from cache
int add_cache_element(char* data, int size, char* url);
void remove_cache_element();

int port_number = 8080; //Selecting port number
int proxy_socketId;

// Creating separete thread for each socket
pthread_t tid[MAX_CLIENTS]; //array of thread ids

sem_t semaphore;
pthread_mutex_t lock;

cache_element* head; //head of linked list
int cache_size;//size of cache

//argc(Argument Count)-> Integer stores total numbers of command line arguments passed to program including program's name itself, initiliases with 1
//argv(Argument Vector)->points to strign representing a single command-line argument
int main(int argc, char* argv[]){
    //client fd, and address len 
    int client_socketID, client_len;

    struct sockaddr_in server_addr, client_addr; //define the socket address socket is of sockaddr structure
    sem_init(&semaphore, 0, MAX_CLIENTS);//Minimum and Maximum value for semaphore
    pthread_mutex_init(&lock, NULL);//By default c initializes with garbage value

    if(argv == 2){
        port_number = atoi(argv[1]);//giving in port, by default command line arguments come in string hence we convert it to int using atoi and pick the second arugrment string that is port number stops
        // ./proxy 8080- [0]th position = ./proxy and [1]st position = 8080
    }else{
        printf("Too few arguments\n");
        exit(1);//Exit with status 1 indicating failure
    }

    printf("Starting Proxy server at port: %d\n", port_number);
    //socket id is server descriptor I think - socket file descriptor it is after socket gets created.
    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);//AF_INTET specifies IPV4,0 says use the system's default protocol, Sock stream indicates type of socket, and stream socket is used for TCP connections.

    if(proxy_socketId < 0){
        perror("Failed to create a socket\n");
        exit(1);
    }

    int reuse = 1;
    // SOL_SOCKET - setting socket level option, &reuse pointer to the value, size of option value
    if(setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse))<0){
        perror("setSockOpt Failed\n");
    }

    bzero((char*)&server_addr, sizeof(server_addr));//clean
    // htons()- converts 16 bit, 32 bit and 64 bit quantities bettween network type order and hots byte order.
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // lots of type casting along with size of required in c
    if(bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        perror("Port is not available\n");
        exit(1);
    }

    printf("Binding on port %d\n", port_number);

    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if(listen_status<0){
        perror("Error in listening\n");
        exit(1);
    }

    int i = 0;
    int Connected_socketID[MAX_CLIENTS];

    while (1)
    {
        bzero((char*)&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketID = accept(proxy_socketId, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
        if(client_socketID<0){
            printf("Not able to connect");
            exit(1);
        }
        else{
            Connected_socketID[i]=client_socketID;
        }
        //40:17
    }
    

}