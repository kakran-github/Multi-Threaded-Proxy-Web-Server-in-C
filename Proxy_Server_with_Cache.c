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
#define MAX_BYTES 4096 
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

// Creating Thread Identifiers Array
pthread_t tid[MAX_CLIENTS];

sem_t semaphore;
pthread_mutex_t lock;

cache_element* head; //head of linked list
int cache_size;//size of cache


int connectRemoteServer(char* host_addr, int port_number){
    //End server se communicate krne ke liye ek socket kholna hoga
    //Creating Socket
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

    //If socket not created
    if(remoteSocket<0){
        printf("Error in creating your socket\n");
        return -1;
    }
    //Look up host in your system
    struct hostent* host = gethostbyname(host_addr);

    //stderr - standard error
    if(host == NULL){
        fprintf(stderr, "No such host exits\n");
        return -1;
    }

    //setting up socket address 
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);

    memcopy((char*)&server_addr.sin_addr.s_addr, (char*)&host->h_addr_list[0],host->h_length);
    
    //size_t is int only - confirm once
    if(connect(remoteSocket, (struct sockaddr*)&server_addr, (size_t)size_of(server_addr))<0){
        fprintf(stderr, "Error in connecting\n");
        return -1;
    }
    return remoteSocket;
}


int handle_request(int clientSocketID, ParsedRequest *request, char* tempReq){
    //allocating buffer
    char* buf = (char*)malloc(sizeof(char)*MAX_BYTES);
    strcpy(buf, "GET ");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");

    size_t len = strlen(buf);
    
    //checking the request recieved as a parameter on socket
    if(ParsedHeader_set(request, "Connection", "close")<0){
        printf("set header key is not working");
    }

    if(ParsedHeader_get(request, "Host") == NULL){
        if(ParsedHeader_set(request, "Host", request->host)<0){
            printf("Set host header key is not working");
        }
    }

    //Setting the port
    int server_port = 80;
    if(request->port != NULL){
        server_port = atoi(request->port);
    }
    // End Server not proxy
    int remoteSocketID = connectRemoteServer(request->host, server_port);
}

//prarameter me client socket ka file descriptor aara h jisse ham http request accept krre hain as a proxy server
void * thread_fn(void *socketNew){
    sem_wait(&semaphore);
    int p;
    sem_getvalue(&semaphore, p);
    printf("Semaphore value is: %d\n",p);

    int *t = (int*) socketNew;
    int socket = *t;//dereferencing the pointer

    //client jo bhi http request marna chata hai uski byte send krega
    int byte_send_client, len;

    //buffer here send queue?
    char* buffer = (char*)calloc(MAX_BYTES, sizeof(char));
    bzero(buffer, MAX_BYTES);//removing garbage value

    //receving bytes from client - receiving on socket created above
    // buffer offset is passed as parameter I think
    byte_send_client = recv(socket, buffer, MAX_BYTES, 0);

    //bytes send from client positive yani hame request aari hain http ki
    while (byte_send_client > 0)
    {
        len = strlen(buffer);//ho skta h buffer me value kam aayi ho buffer full na kiya ho
        if(strstr(buffer, "\r\n\r\n")==NULL){ //HTTP request always ends with "\r\n\r\n"
            //accept untill you get the end of file mentioned above
            byte_send_client = recv(socket, buffer + len, MAX_BYTES-len, 0);
        }
        else{
            break;
        }
    }
    //Client se request aagyi proxy tk

    //Now we need to search the request in cache first
    //Making a copy to search in cache (good practice only)
    char *tempReq = (char*) malloc (strlen(buffer)*sizeof(char)*1); //jitna buffer h multiply size of character
    for(int i = 0; i<strlen(buffer); i++){
        tempReq[i] = buffer[i];
    }

    //using find function here declared above
    cache_element* temp = find(tempReq);
    // If found in cache
    if(temp!=NULL){
        int size = temp->len/sizeof(char);
        int pos =0;
        char response[MAX_BYTES];
        while(pos<size){
            bzero(response, MAX_BYTES);
            for(int i=0;i<MAX_BYTES;i++){
                response[i]=temp->data[i];
                pos++;
            }
            send(socket, response, MAX_BYTES, 0);
        }
        printf("Data retrieved from the cache\n");
        printf("%s\n\n", response);
    }
    //Not found in cache retrieve from the server
    else if(byte_send_client>0){
        len = strlen(buffer);
        //Headers and all fetch krne ke liye parsed request
        //Its a struct with information such as method, protocol, host, etc
        ParsedRequest* request = ParsedRequest_create();

        if(ParsedRequest_parse(request, buffer, len)<0){
            printf("Parsing Failed")
        }
        else{
            bzero(buffer, MAX_BYTES);
            if(!strcmp(request->method, "GET")){
                if(request->host && request->path && checkHTTPversion(request->version)==1){
                    byte_send_client = handle_request(socket, request, tempReq);
                    //No response even after handle request function from proxy server
                    if(byte_send_client == -1){
                        sendErrorMessage(socket, 500);
                    }
                }
                else{
                    //if actual main server didn't respond either
                        sendErrorMessage(socket, 500);
                    }
            }
            else{
                printf("This code dosen't support any method apart from GET");
            }
        }
        ParsedRequest_destroy(request);
    }
    else if(byte_send_client == 0){
            printf("Client is disconnected");
    }
    shutdown(socket, SHUT_RDWR);//close socket shut read write
    close(socket);//additional which we made for function
    free(buffer);
    sem_post(&semaphore);

    sem_getvalue(&semaphore, p);
    printf("Semaphore post value is %d\n", p);
    free(tempReq);
    return NULL;
}





//argc(Argument Count)-> Integer stores total numbers of command line arguments passed to program including program's name itself, initiliases with 1
//argv(Argument Vector)->points to strign representing a single command-line argument
int main(int argc, char* argv[]){
    //client fd 
    int client_socketID, client_len;

    // define server address to pass as parameter in socket creation
    struct sockaddr_in server_addr, client_addr;

    // initializing semaphore and mutex
    sem_init(&semaphore, 0, MAX_CLIENTS);//Minimum and Maximum value for semaphore
    pthread_mutex_init(&lock, NULL);//By default c initializes with garbage value

    //Setting Port Number for socket
    if(argv == 2){
        port_number = atoi(argv[1]);//giving in port, by default command line arguments come in string hence we convert it to int using atoi and pick the second arugrment string that is port number stops
        // ./proxy 8080- [0]th position = ./proxy and [1]st position = 8080
    }else{
        printf("Too few arguments\n");
        exit(1);//Exit with status 1 indicating failure
    }

    printf("Starting Proxy server at port: %d\n", port_number);

    //Creating Proxy Server Socket
    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);//AF_INET specifies IPV4,0 says use the system's default protocol, Sock stream indicates type of socket, and stream socket is used for TCP connections.

    if(proxy_socketId < 0){
        perror("Failed to create a socket\n");
        exit(1);
    }

    int reuse = 1;
    // SOL_SOCKET - setting socket level option, &reuse pointer to the value, size of option value
    if(setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse))<0){
        perror("setSockOpt Failed\n");
    }
    
    //Ensures all values sin_zero, sin_port, etc start off clean with zero preventing garbage values.
    bzero((char*)&server_addr, sizeof(server_addr));//clean
    
    // Setting Server Address
    // htons()- converts 16 bit, 32 bit and 64 bit quantities bettween network type order and hots byte order.
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to port
    if(bind(proxy_socketId, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        perror("Port is not available\n");
        exit(1);
    }

    printf("Binding on port %d\n", port_number);

    // Listen status yes or no
    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if(listen_status<0){
        perror("Error in listening\n");
        exit(1);
    }

    int i = 0;
    //How much sockets are connected with proxy
    int Connected_socketID[MAX_CLIENTS];

    while (1)
    {
        client_len = sizeof(client_addr);
        bzero((char*)&client_addr, client_len);

        // Listening on proxysocket, kaha se kaha aara h client addr se proxy socket tk to isliye address client ka h
        client_socketID = accept(proxy_socketId, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);//this is just typecasting
        if(client_socketID<0){
            printf("Not able to connect");
            exit(1);
        }
        else{
            //Socket accept hogya to connection fd store krlo jo bhi proxy server se connect hua h
            Connected_socketID[i]=client_socketID;
        }
        
        //Printing the connected client details
        struct sockaddr_in * client_pt = (struct sockaddr_in *)&client_addr;//Pointer to client addr
        struct in_addr ip_addr = client_pt->sin_addr;//copying IP address
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);//type casting IP address
        printf("Client is connected with port number %d and ip address is %s\n", ntohs(client_addr.sin_port), str);

        //Creating thread for each connection which is connectino socket ID, thread_fn we will create above, for thread id we already created array above.
        pthread_create(&tid[i], NULL, thread_fn, (void *)&Connected_socketID[i]);
        i++;// increment the loop
    }
    //close the proxy socket after done
    close(proxy_socketId);
    
    return 0;
}