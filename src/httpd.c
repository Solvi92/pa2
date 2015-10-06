/* A UDP echo server with timeouts.
 *
 * Note that you will not need to use select and the timeout for a
 * tftp server. However, select is also useful if you want to receive
 * from multiple sockets at the same time. Read the documentation for
 * select on how to do this (Hint: Iterate with FD_ISSET()).
 */

#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <time.h>

typedef struct {
    char* type;         // GET - POST - Header
    char* file;         // Requested file
    char* httpVersion;  // Http version
    char* body;         // Contents of a POST request
    char* userAgent;
    char* host;
    char* acceptLanguage;
} ClientHeader;

typedef struct {
    int clientIP;
    short clientPort;
    char* requestMethod;
    char* requestedURL;
    char* responseCode;
} LogInfo;

GString *RESPONSE_HEAD;
LogInfo* logInfo;

void createHead(int contentLength) {
    /* Creating the date format */
    char date[100];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(date, sizeof(date)-1, "%a, %d %h %Y %H:%M:%S GMT", t);
    char c[16];
    sprintf(c, "%d", contentLength);
    RESPONSE_HEAD = g_string_new("HTTP/1.1 200 OK\r\nDate: ");
    RESPONSE_HEAD = g_string_append(RESPONSE_HEAD, date);
    RESPONSE_HEAD = g_string_append(RESPONSE_HEAD, "\r\nContent-Type: text/html\r\nContent-Length: ");
    RESPONSE_HEAD = g_string_append(RESPONSE_HEAD, c);
    RESPONSE_HEAD = g_string_append(RESPONSE_HEAD, "\r\n\r\n");
}

void handleGET(int connfd) {
    /* Send the message back. */
    GString *page = g_string_new("<!doctype html>\n<head>\n<title>Jolly good</title>\n</head>");
    page = g_string_append(page, "<body>\nHello mates! Welcome to our web site\n</body>\n</html>");

    createHead(page->len);
    page = g_string_prepend(page, RESPONSE_HEAD->str);

    write(connfd, page->str, (size_t) page->len);
    g_string_free(page, 1);
    g_string_free(RESPONSE_HEAD, 1);
}

void handlePOST(int connfd, ClientHeader *clientHeader) {
    /* Send the message back. */
    GString *page = g_string_new("<!doctype html>\n<head>\n<title>Jolly good</title>\n</head>\n<body>\n");
    page = g_string_append(page, clientHeader->body);
    page = g_string_append(page,"\n</body> \n</html>");

    createHead(page->len);
    page = g_string_prepend(page, RESPONSE_HEAD->str);

    write(connfd, page->str, (size_t) page->len);
    g_string_free(page, 1);
    g_string_free(RESPONSE_HEAD, 1);
}

void handleHEAD(int connfd) {
    createHead(0);
    write(connfd, RESPONSE_HEAD->str, RESPONSE_HEAD->len);
    g_string_free(RESPONSE_HEAD, 1);
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in server, client;
    char message[512];
    logInfo = g_new0(LogInfo, 1);

    /* Create and bind a UDP socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    /* Network functions need arguments in network byte order instead of
       host byte order. The macros htonl, htons convert the values, */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(7309);
    bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));

	/* Before we can accept messages, we have to listen to the port. We allow one
	 * 1 connection to queue for simplicity.
	 */
	listen(sockfd, 1);

    for (;;) {
        fd_set rfds;
        struct timeval tv;
        int retval;

        /* Check whether there is data on the socket fd. */
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);

        /* Wait for five seconds. */
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        retval = select(sockfd + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1) {
            perror("select()");
        } else if (retval > 0) {
            /* Data is available, receive it. */
            assert(FD_ISSET(sockfd, &rfds));

            /* Copy to len, since recvfrom may change it. */
            socklen_t len = (socklen_t) sizeof(client);

            /* For TCP connectios, we first have to accept. */
            int connfd;
            connfd = accept(sockfd, (struct sockaddr *) &client,
                            &len);
            ssize_t n = read(connfd, message, sizeof(message) - 1);
            
            /* Getting information about the user
            Right now, the IP address is very wrong */
            logInfo->clientIP = inet_ntoa(client.sin_addr);
            logInfo->clientPort = ntohs(client.sin_port);


            /* Building a struct of header arguments */
            ClientHeader* clientHeader = g_new0(ClientHeader, 1);
            gchar **reqlist = g_strsplit(message, "\r\n", 100);

            clientHeader->type = strtok(reqlist[0], " \t\n");
            clientHeader->file = strtok(NULL, " \t");
            clientHeader->httpVersion = strtok(NULL, " \t\n");

            int i = 1;
            while(reqlist[i] != NULL) {
                gchar **linesplit = g_strsplit(reqlist[i], ":", 100);

                if(g_strv_length(linesplit) == 0) {
                    // We reached an empty line and the body is in the next line.
                    clientHeader->body = reqlist[i + 1];
                }

                ++i;
                g_strfreev(linesplit);
            }

            logInfo->requestMethod = clientHeader->type;
            logInfo->requestedURL = clientHeader->file;

            if(g_strcmp0(clientHeader->httpVersion, "HTTP/1.0") != 0 && g_strcmp0(clientHeader->httpVersion, "HTTP/1.1") != 0) {
                write(connfd, "HTTP/1.0 400 Bad Request\n", (size_t) 512);
                logInfo->responseCode = "400 Bad Request";
            }
            else {
                
                logInfo->responseCode = "200 OK";
                if(g_strcmp0(clientHeader->type, "GET") == 0) {
                    handleGET(connfd);
                }
                else if(g_strcmp0(clientHeader->type, "POST") == 0) {
                    handlePOST(connfd, clientHeader);
                }
                else if(g_strcmp0(clientHeader->type, "HEAD") == 0) {
                    handleHEAD(connfd);
                }
            }

            g_strfreev(reqlist);
            /* We should close the connection. */
            shutdown(connfd, SHUT_RDWR);
            close(connfd);

            /* Print the message to stdout and flush. */
            fprintf(stdout, "Received:\n%s\n", message);
            fflush(stdout);

            g_free(clientHeader);
            g_free(logInfo);
        } else {
            fprintf(stdout, "No message in five seconds.\n");
            fflush(stdout);
        }
    }
}
