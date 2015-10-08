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
#include <arpa/inet.h>

typedef struct {
    char* type;         // GET - POST - Header
    char* file;         // Requested file
    char* httpVersion;  // Http version
    char* body;         // Contents of a POST request
    GSList *addHdrs;    // Additional header fields
} ClientHeader;

typedef struct {
    char clientIP[64];
    char clientPort[8];
    char requestMethod[16];
    char requestedURL[32];
    char responseCode[32];
} LogInfo;

GString *RESPONSE_HEAD;
LogInfo* logInfo;

void writeToLog() {
    FILE *logFile;
    logFile = fopen("httpd.log", "a");

    time_t timestamp;
    time(&timestamp);
    char buf[sizeof("2011-10-08T07:07:09Z")];
    strftime(buf, sizeof(buf), "%FT%TZ", gmtime(&timestamp));

    GString *stringBuilder = g_string_new(buf);
    stringBuilder = g_string_append(stringBuilder, " : ");
    stringBuilder = g_string_append(stringBuilder, logInfo->clientIP);
    stringBuilder = g_string_append(stringBuilder, ":");
    stringBuilder = g_string_append(stringBuilder, logInfo->clientPort);
    stringBuilder = g_string_append(stringBuilder, " ");
    stringBuilder = g_string_append(stringBuilder, logInfo->requestMethod);
    stringBuilder = g_string_append(stringBuilder, " ");
    stringBuilder = g_string_append(stringBuilder, logInfo->requestedURL);
    stringBuilder = g_string_append(stringBuilder, " : ");
    stringBuilder = g_string_append(stringBuilder, logInfo->responseCode);

    /* timestamp : <client ip>:<client port> <request method>
    <requested URL> : <response code> */

    if(logFile == NULL) {
        printf("Error when opening file mates :>");
    }
    else {
        fprintf(logFile, "%s\n", stringBuilder->str);
    }

    fclose(logFile);
    g_string_free(stringBuilder, 1);
}

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
}

void handleGET(int connfd, ClientHeader *clientHeader) {
    /* Creating the page */
    GString *page = g_string_new("<!doctype html>\n<head>\n<title>Jolly good</title>\n</head>\n");
    GString *cookie = g_string_new("");
    GString *clientCookie = g_string_new("");

    /* Checking if the client supplied a cookie before we do anything */
    if(clientHeader->addHdrs != NULL) {
        /* Traverse through the headers and look for a cookie header */
        GSList *header = clientHeader->addHdrs;
        int k = 0;
        for(k; k < g_slist_length(clientHeader->addHdrs); ++k) {
            char *bg;
            if((bg = strstr(header->data, "Cookie")) != NULL) {
                printf("Cookie found\n");
                gchar **beforeAndCK = g_strsplit(bg, "&", -1);
                gchar **bgSplitCK = g_strsplit(beforeAndCK[0], "=", -1);

                printf("Cookie var: %s\n", bgSplitCK[1]);
                g_string_append(clientCookie, bgSplitCK[1]);

                g_strfreev(beforeAndCK);
                g_strfreev(bgSplitCK);
            }

            header = header->next;
        }
    }

    /* Checking to see if any query parameters are following */
    gchar **list = g_strsplit(logInfo->requestedURL, "?", -1);
    if(g_strv_length(list) > 1 || clientCookie->len > 0) {
        /* Leaving body open in case color has to be added */
        g_string_append(page, "<body");

        /* Checking to see if color has been specified */
        if(strstr(list[0], "color") != NULL) {
            char *bg;
            /* No query parameters supplied, using the cookie */
            if(list[1] == NULL && clientCookie->len > 0) {
                g_string_append(page, " style='background-color:");
                g_string_append(page, clientCookie->str);
                g_string_append(page, "'");
            }
            /* Finding the word bg in the query and assigning the color with it */
            else if((bg = strstr(list[1], "bg=")) != NULL) {
                gchar **beforeAnd = g_strsplit(bg, "&", -1);
                gchar **bgSplit = g_strsplit(beforeAnd[0], "=", -1);

                /* Appending the background color to the <body> tag */
                g_string_append(page, " style='background-color:");
                g_string_append(page, bgSplit[1]);
                g_string_append(page, "'");

                /* Appending the cookie colour to the head */
                cookie = g_string_new("Set-Cookie: bg=");
                g_string_append(cookie, bgSplit[1]);
                g_string_append(cookie, "\n");

                g_strfreev(beforeAnd);
                g_strfreev(bgSplit);
            }
        }

        /* Appending the closing tag of <body> */
        g_string_append(page, ">\n");

        if(strstr(list[0], "test") != NULL) {
            /* Query parameters found in the url */
            gchar **queryList = g_strsplit(list[1], "&", -1);
            /* Adding a p tag to the site containing all the query parameters */
            page = g_string_append(page, "<p>\n");
            int i = 0;
            /* For each query we create a line in the paragraph */
            for(i; i < g_strv_length(queryList); ++i) {
                page = g_string_append(page, queryList[i]);
                page = g_string_append(page, " ");
                page = g_string_append(page, "</br>\n");
            }
            page = g_string_append(page, page->str);
            page = g_string_append(page, "</p>");

            g_strfreev(queryList);
        }
        
    }
    else {
        /* No query parameters, return the generic html */
        g_string_append(page, "<body>\n");
    }

    /* Printing out additional headers on the page */
    if(clientHeader->addHdrs != NULL) {
        page = g_string_append(page, "<p>\n");
        GSList *header = clientHeader->addHdrs;
        int k = 0;
        for(k; k < g_slist_length(clientHeader->addHdrs); ++k) {
            page = g_string_append(page, header->data);
            page = g_string_append(page, " </ br>\n");

            header = header->next;
        }

        page = g_string_append(page, "</p>\n");
    }

    page = g_string_append(page, "\nHello mates! Welcome to our web site\n</body>\n</html>\n\n");


    /* Creating the http header and prepending it to the page */
    createHead(page->len);
    g_string_append(RESPONSE_HEAD, "\r\n");
    g_string_append(RESPONSE_HEAD, cookie->str);
    g_string_append(RESPONSE_HEAD, "\r\n\r\n");
    g_string_prepend(page, RESPONSE_HEAD->str);
    printf("Header:\n%s", RESPONSE_HEAD->str);
    
    /* Sending the page to the client */
    write(connfd, page->str, (size_t) page->len);


    /* Logging down the user information */
    writeToLog();
    
    /* Free */
    g_string_free(clientCookie, 1);
    g_string_free(cookie, 1);
    g_string_free(page, 1);
    g_string_free(RESPONSE_HEAD, 1);
    g_strfreev(list);
}

void handlePOST(int connfd, ClientHeader *clientHeader) {
    /* Creating the page */
    GString *page = g_string_new("<!doctype html>\n<head>\n<title>Jolly good</title>\n</head>\n<body>\n");
    page = g_string_append(page, clientHeader->body);
    page = g_string_append(page,"\n</body> \n</html>");

    /* Creating the http header and prepending it to the page */
    createHead(page->len);
    RESPONSE_HEAD = g_string_append(RESPONSE_HEAD, "\r\n\r\n");
    page = g_string_prepend(page, RESPONSE_HEAD->str);

    /* Sending the page to the client */
    write(connfd, page->str, (size_t) page->len);

    /* Free */
    g_string_free(page, 1);
    g_string_free(RESPONSE_HEAD, 1);

    /* Logging down the user information */
    writeToLog();
}

void handleHEAD(int connfd) {
    /* Creating the http header and sending it to the client */
    createHead(0);
    RESPONSE_HEAD = g_string_append(RESPONSE_HEAD, "\r\n\r\n");
    write(connfd, RESPONSE_HEAD->str, RESPONSE_HEAD->len);

    /* Free */
    g_string_free(RESPONSE_HEAD, 1);

    /* Logging down the user information */
    writeToLog();
}


int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in server, client;
    char message[512];

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
            
            /* Getting logging information about the user */
            logInfo = g_new0(LogInfo, 1);
            strcpy(logInfo->clientIP, inet_ntoa(client.sin_addr));
            sprintf(logInfo->clientPort, "%d", ntohs(client.sin_port));

            /* Building a struct of header arguments */
            ClientHeader* clientHeader = g_new0(ClientHeader, 1);
            gchar **reqlist = g_strsplit(message, "\r\n", 100);
            clientHeader->type = strtok(reqlist[0], " \t\n");
            clientHeader->file = strtok(NULL, " \t");
            clientHeader->httpVersion = strtok(NULL, " \t\n");
            clientHeader->addHdrs = NULL;

            int i = 1;
            while(reqlist[i] != NULL) {
                gchar **linesplit = g_strsplit(reqlist[i], ":", 100);

                if(g_strv_length(linesplit) == 0) {
                    /* We reached an empty line and the body is in the next line. */
                    clientHeader->body = reqlist[i + 1];
                    break;
                }
                else {
                    /* We add the additional header to the struct */
                    clientHeader->addHdrs = g_slist_append(clientHeader->addHdrs, reqlist[i]);
                }
                ++i;
                g_strfreev(linesplit);
            }

            /* Getting logging information about the request */
            strcpy(logInfo->requestMethod, clientHeader->type);
            strcpy(logInfo->requestedURL, clientHeader->file);

            if(g_strcmp0(clientHeader->httpVersion, "HTTP/1.0") != 0 && g_strcmp0(clientHeader->httpVersion, "HTTP/1.1") != 0) {
                write(connfd, "HTTP/1.0 400 Bad Request\n", (size_t) 512);
                strcpy(logInfo->responseCode, "400 Bad Request");
            }
            else {
                /* Everything is in order, handle the request */
                strcpy(logInfo->responseCode, "200 OK");
                if(g_strcmp0(clientHeader->type, "GET") == 0) {
                    handleGET(connfd, clientHeader);
                }
                else if(g_strcmp0(clientHeader->type, "POST") == 0) {
                    handlePOST(connfd, clientHeader);
                }
                else if(g_strcmp0(clientHeader->type, "HEAD") == 0) {
                    handleHEAD(connfd);
                }
                g_free(logInfo);
                g_strfreev(reqlist);
                g_free(clientHeader);
            }
            /* Freeing the allocated memory */

            /* We should close the connection. */
            shutdown(connfd, SHUT_RDWR);
            close(connfd);

            /* Print the message to stdout and flush. */
            // fprintf(stdout, "Received:\n%s\n", message);
            fflush(stdout);

        } else {
            fprintf(stdout, "No message in five seconds.\n");
            fflush(stdout);
        }
    }
}
