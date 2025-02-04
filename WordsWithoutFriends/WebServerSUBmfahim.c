#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netdb.h>

#define SHORT_BUFFER_LEN 1024
#define FILE_BUFFER_LEN (1024 * 1024)
#define STR_SERVER_PORT_NUMBER "8000"

const char *strRootDirectory = NULL; // this will be set in main

// reads file contents and creates response string. returns 404 if file does not exist.
void createResponseString(const char *strFileName, char *strResponse, size_t *ptrResponseLen) {
    int fd = open(strFileName, O_RDONLY);

    if (fd == -1)                       // in case file does not exist, then send 404 Not Found
        *ptrResponseLen = snprintf(strResponse, FILE_BUFFER_LEN, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found");
    else {                              // if file exists, then send contents of file
        struct stat st;
        stat(strFileName, &st);
        const char * const response_header = "HTTP/1.1 200 OK\r\nContent-Length: ";
        *ptrResponseLen += snprintf(strResponse, FILE_BUFFER_LEN, "%s%ld\r\n\r\n", response_header, st.st_size);

        ssize_t bytes_read;
        while ((bytes_read = read(fd, strResponse + *ptrResponseLen, FILE_BUFFER_LEN - *ptrResponseLen)) > 0)
            *ptrResponseLen += bytes_read;
        close(fd);
    }
}

// for one client connection, this functions is invoked in a separate thread.
void *requestHandlerFunction(void *arg) {
    int fd_socket = *((int *)arg);
    char strRequest[SHORT_BUFFER_LEN], strFullFilename[SHORT_BUFFER_LEN];

    // read and analyze request from client
    ssize_t nRequestBytes = recv(fd_socket, strRequest, SHORT_BUFFER_LEN, 0);
    const int GET_LEN = 5;
    if( nRequestBytes > GET_LEN && !strncmp("GET /", strRequest, GET_LEN)) {
        int i = GET_LEN;
        while( *(strRequest + i++) != ' ');
        strRequest[i-1] = '\0';

        // create strResponse
        char *strResponse = (char *)malloc(FILE_BUFFER_LEN * 2 * sizeof(char));
        size_t nResponseLen = 0;

        snprintf( strFullFilename, 1024, "%s/%s", strRootDirectory, strRequest + GET_LEN);
        printf("Returning to client: filename: %s\n", strFullFilename);

        createResponseString(strFullFilename, strResponse, &nResponseLen);

        // send strResponse back to client and then free it
        send(fd_socket, strResponse, nResponseLen, 0);
        free(strResponse);
    }

    close(fd_socket);
    free(arg);
    return NULL;
}

// main function: takes the root file directory as first argument, from which the files are served by the server
int main(int argc, char *argv[]) {
    if( argc != 2) {
        printf("Usage: <%s> <root file directory>", argv[0]);
        return -1;
    }

    strRootDirectory = argv[1];
    printf("Going to use root directory to server files: %s\n", strRootDirectory);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    getaddrinfo(NULL, STR_SERVER_PORT_NUMBER, &hints, &res);
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    // bind socket to SERVER_PORT_NUMBER
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind failed");
        return -1;
    }

    // listen for connections
    if (listen(sockfd, 10) < 0) {
        perror("listen failed");
        return -1;
    }

    printf("Server listening on port %s on localhost. Access it using localhost:8000/<filename>\n", STR_SERVER_PORT_NUMBER);
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int *fdClient     = malloc(sizeof(int));

        // accept client connection
        if ((*fdClient = accept(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen)) < 0) {
            perror("accept failed");
            continue;
        }

        // create a new thread to handle client request
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, requestHandlerFunction, (void *)fdClient);
        pthread_detach(thread_id);
    }

    close(sockfd);
    return 0;
}
