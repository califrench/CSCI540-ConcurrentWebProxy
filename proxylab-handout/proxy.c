/**
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

#define MAX_THREADS 30


typedef struct {
    struct sockaddr_in addr;
    int sockfd;
} data_t;


/**
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void start_listening(int port);
void forward_request(char *hostname, char *pathname, int port, char *request, int dsockfd, int t_id);
void *handle_socket(void *thread_id);

pthread_mutex_t logfile_mutex;
pthread_mutex_t gethost_mutex;
FILE *fh;
data_t thread_data[MAX_THREADS];



/** 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv) {
    int port;
    //, listenfd, clientfd;
    // struct rio_t listenb, clientb;
    // char buffer[MAXLINE];
    
    /* Check arguments */
    if (argc != 2) {
       fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
       exit(0);
    }

    port = atoi(argv[1]);
    printf("got port %d for listen\n", port);

    pthread_mutex_init(&logfile_mutex, NULL);
    pthread_mutex_init(&gethost_mutex, NULL);

    fh = fopen("proxy.log", "a+");

    start_listening(port);

    exit(0);
}

void start_listening(int port) {
    int sockfd, newsockfd, i, t;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    pthread_t threads[MAX_THREADS];
    

    /* First call to socket() function */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    /* Initialize socket structure */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    /* Now bind the host address using bind() call.*/
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    /* Now start listening for the clients, here process will
    * go in sleep mode and will wait for the incoming connection
    */

    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    i = 0;
    t = 0;
    
    while (1) {
        /* Accept actual connection from the client */
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }

        pthread_create(threads + i, NULL, handle_socket, (void *)(long int)i);

        thread_data[i].addr = cli_addr;
        thread_data[i].sockfd = newsockfd;

        i = (i+1)%MAX_THREADS;
        t++;
        if (t >= MAX_THREADS) {
            printf("will to join\n");
            pthread_join(threads[(MAX_THREADS + i-t) % MAX_THREADS], NULL);
            printf("did to join\n");
            t--;
        }
        printf("got %d threads\n", t);
    }
}

void *handle_socket(void *thread_id_p) {
    char buffer[MAXLINE], f_hostname[MAXLINE], f_pathname[MAXLINE], uri[MAXLINE];
    char *end;
    int n, f_port;
    int t_id = (long int)thread_id_p;
    data_t data = thread_data[t_id];
    int newsockfd = data.sockfd;

    /* If connection is established then start communicating */
    bzero(buffer, MAXLINE);
    printf("will read from client\n");
    n = read(newsockfd, buffer, MAXLINE-1);
    printf("did read from client\n");

    if (n < 0) {
        perror("ERROR reading from socket");
        exit(1);
    }
    if (!n) {
        return NULL;
    }

    // printf("Proxy got %d: %s\n",n, buffer);

    // printf("Need to parse URI and forward request now...\n");
    bzero(f_hostname, MAXLINE);
    bzero(f_pathname, MAXLINE);

    end = strpbrk(buffer+4, " ");
    strncpy(uri, buffer+4, end-buffer-4);

    if (parse_uri(uri, f_hostname, f_pathname, &f_port) > -1) {
        forward_request(f_hostname, f_pathname, f_port, buffer, newsockfd, t_id);
    } else {
        printf("parse URI failed:%s\n", uri);
    }
    return NULL;
}

void forward_request(char *hostname, char *pathname, int port, char *request, int dsockfd, int t_id) {
    int sockfd, n, n2;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    // char *content_size;
    char buffer[MAXLINE], logstring[MAXLINE], uri[MAXLINE];
    int ret = 0;
    fd_set fdset;
    struct timeval tv;

    bzero(uri, MAXLINE);
    char * end = strpbrk(request+4, " ");
    strncpy(uri, request+4, end-request-4);

    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) {
       perror("ERROR opening socket");
       exit(1);
    }
    
    printf("will get host by name\n");
    pthread_mutex_lock(&gethost_mutex);
    server = gethostbyname(hostname);
    pthread_mutex_unlock(&gethost_mutex);
    printf("did get host by name\n");

    if (server == NULL) {
       fprintf(stderr,"ERROR, no such host\n");
       exit(0);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    /* Now connect to the server */
    printf("Connecting to server %s\n", hostname);
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
       perror("ERROR connecting");
       exit(1);
    }
    printf("Connected to server\n");

    
    /* Send message to the server */
    // bzero(buffer, MAXLINE);
    // sprintf(buffer, "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n", pathname, hostname);

    // printf("About to send request to server:\n%s\n", buffer);
    printf("Sending request to server\n");
    n = write(sockfd, request, strlen(request));
    printf("Send request to server\n");
    
    if (n < 0) {
       perror("ERROR writing to socket");
       exit(1);
    }

    printf("Sent request to server %s/%s\n", hostname, pathname);
    
    /* Now read server response */
    

    // printf("About to read response\n");
    // printf("%lu\n", strlen(buffer));
    long unsigned int count = 0;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);

    printf("Will wait\n");
    if ((ret = select(sockfd + 1, &fdset, NULL, NULL, &tv)) <= 0) {
        close(sockfd);
        close(dsockfd);
        printf("Did wait failure\n");
        return;
    }
    printf("Did wait success\n");

    //ret must be positive
    if(FD_ISSET(sockfd, &fdset)) {
        bzero(buffer,MAXLINE);
        printf("Will read 1\n");
        n = read(sockfd, buffer, MAXLINE-1);
        printf("Did read 1\n");
        while (n > 0) {
            printf("Will write\n");
            n2 = write(dsockfd,buffer,n);
            printf("Did write\n");
            if (n2 < 0) {
                perror("ERROR writing to socket");
                exit(1);
            }
            count += n;
            bzero(buffer,MAXLINE);

            printf("Will wait 2\n");
            if ((ret = select(sockfd + 1, &fdset, NULL, NULL, &tv)) <= 0) {
                close(sockfd);
                close(dsockfd);
                printf("Did wait 2 failure\n");
                return;
            }
            printf("Did wait 2 success\n");

            if(FD_ISSET(sockfd, &fdset)) {
                bzero(buffer,MAXLINE);
                printf("Will read 2\n");
                n = read(sockfd, buffer, MAXLINE-1);
                printf("Did read 2\n");
            } else {
                close(sockfd);
                close(dsockfd);
                printf("Did wait 2 failure\n");
                return;
            }
        }
        if (n < 0) {
            perror("ERROR reading from socket");
            exit(1);
        }


        format_log_entry(logstring, &thread_data[t_id].addr, uri, count);
        pthread_mutex_lock(&logfile_mutex);
        fprintf(fh, "%s\n", logstring);
        pthread_mutex_unlock(&logfile_mutex);
    }
    printf("Will close\n");
    close(sockfd);
    close(dsockfd);
    printf("Did close\n");

}



/**
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port) {
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0 && strncasecmp(uri, "https://", 8) != 0) {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    *port = 80; /* default */

    if (*hostend == ':')   
        *port = atoi(hostend + 1);
    if (strncasecmp(uri, "https://", 8) == 0) {
        printf("%s\n", uri);
        *port = 443;
    }
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/**
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size) {
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /**
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}

