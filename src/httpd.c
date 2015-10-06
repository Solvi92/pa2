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

typedef struct {
    char* type;         // GET - POST - Header
    char* file;         // Requested file
    char* httpVersion;  // Http version
    char* body;         // Contents of a POST request
    char* userAgent;
    char* host;
    char* acceptLanguage;
} ClientHeader;

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

            /* Building a struct of header arguments */
            ClientHeader* clientHeader = (ClientHeader*) malloc(sizeof(ClientHeader));
            gchar **reqlist = g_strsplit(message, "\r\n", 100);

            clientHeader->type = strtok(reqlist[0], " \t\n");
            clientHeader->file = strtok(NULL, " \t");
            clientHeader->httpVersion = strtok(NULL, " \t\n");

            printf("Client header type: %s\n", clientHeader->type);
            printf("Client header file: %s\n", clientHeader->file);
            printf("Client header version: %s\n", clientHeader->httpVersion);

            int i = 1;
            while(reqlist[i] != NULL) {
                gchar **linesplit = g_strsplit(reqlist[i], ":", 100);

                if(g_strv_length(linesplit) == 0) {
                    // We reached an empty line and the body is in the next line.
                    clientHeader->body = reqlist[i + 1];
                }

                ++i;
            }

            if(g_strcmp0(clientHeader->httpVersion, "HTTP/1.0") != 0 && g_strcmp0(clientHeader->httpVersion, "HTTP/1.1") != 0) {
                write(connfd, "HTTP/1.0 400 Bad Request\n", (size_t) 512);
            }
            else {
                if(g_strcmp0(clientHeader->type, "GET") == 0) {
                    /* Send the message back. */
                    char page[512] = "<!doctype html>"
                                    "<head>"
                                        "<title>Jolly good</title>"
                                    "</head>"
                                    "<body>"
                                        "Hello mates! Welcome to our web site"
                                    "</body>"
                                    "</html>";
                    printf("get\n");
                    write(connfd, page, (size_t) 512);
                }
                else if(g_strcmp0(clientHeader->type, "POST") == 0) {
                    /* Send the message back. */
                    GSList* returnMsg = NULL;
                    returnMsg = g_slist_append(returnMsg, "<!doctype html>"
                                    "<head>"
                                        "<title>POST POST</title>"
                                    "</head>" 
                                    "<body>");
                    returnMsg = g_slist_append(returnMsg, clientHeader->body);
                    returnMsg = g_slist_append(returnMsg,"</body> \n</html>");
                    gchar *fullmsg = g_strconcat(g_slist_nth_data(returnMsg, 0), g_slist_nth_data(returnMsg, 1), g_slist_nth_data(returnMsg, 2));
                    printf("Full msg: %s\n", fullmsg);
                    //write(connfd, returnMsg, (size_t) 512);
                }
                else if(g_strcmp0(clientHeader->type, "HEAD") == 0) {
                }
            }
            
            /* We should close the connection. */
            shutdown(connfd, SHUT_RDWR);
            close(connfd);

            /* Print the message to stdout and flush. */
            fprintf(stdout, "Received:\n%s\n", message);
            fflush(stdout);
        } else {
            fprintf(stdout, "No message in five seconds.\n");
            fflush(stdout);
        }
    }
}
