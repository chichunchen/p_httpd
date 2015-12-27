#include "client.h"

#define BUFSIZE 1000
char buf[BUFSIZE];

/* setup connection by given index of requests */
void setup_connection(int index)
{
    int sockfd;
    struct sockaddr_in serveraddr;
    struct hostent *server;

    /* create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_err("opening socket");
    }

    /* get server */
    server = gethostbyname(requests[index].sock_ip);
    if (server == NULL) {
        fprintf(stderr,"error, no such sock host");

        // TODO: hostname
        strcpy(buf, "wrong hostname(sock)<br />");
        write_content_at(index, 'm', buf, 0);

        return;
    }

    /* setup socket */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length); // copy from param1 -> param2
    serveraddr.sin_port = htons(atoi(requests[index].sock_port));

    /* connect: create a connection with the server */
    if (connect(sockfd, (struct sockaddr *)&serveraddr,sizeof(serveraddr)) < 0) {
        log_err("connect to sock server");
        return;
    }

    /* send SOCKS4 request */
    char package[8];
    struct hostent *sh;
    package[0] = 4;
    package[1] = 1;
    package[2] = atoi(requests[index].port) / 256;
    package[3] = atoi(requests[index].port) % 256;
    sh = gethostbyname(requests[index].sock_ip);
    fprintf(stderr, "sh->> %u\n", (unsigned char)sh->h_addr_list[0][0]);
    fprintf(stderr, "sh->> %u\n", (unsigned char)sh->h_addr_list[0][1]);
    fprintf(stderr, "sh->> %u\n", (unsigned char)sh->h_addr_list[0][2]);
    fprintf(stderr, "sh->> %u\n", (unsigned char)sh->h_addr_list[0][3]);
    package[4] = (unsigned char)sh->h_addr_list[0][0];
    package[5] = (unsigned char)sh->h_addr_list[0][1];
    package[6] = (unsigned char)sh->h_addr_list[0][2];
    package[7] = (unsigned char)sh->h_addr_list[0][3];

    Writen(sockfd, package, sizeof(package));

    if( read(sockfd, package, sizeof(package)) <0 ) {
        perror("read sock reply error");
    }

    if( package[1] == 91 ) {
        perror("sock return error");
    }

    /* save socket */
    requests[index].socket = sockfd;

    /* open and store fp */
    char filepath[100];
    strcpy(filepath, "../www/test/");               // should check the path if program break
    strcat(filepath, requests[index].filename);
    requests[index].fp = fopen(filepath, "r");
    if (requests[index].fp == NULL) {
        fprintf(stderr, "open file");
        exit(EXIT_FAILURE);
    }

//     if (DEBUG) {
//         /* print cwd */
//         printf("filepath=%s\n", filepath);
//         char buff[100], *dir;
//         dir = getcwd(buff, 100);
//         printf("dir=%s", dir);

        /* print socket, fp */
        printf("request[%d] socket=%d, fp=%d<br>",
                index, requests[index].socket, fileno(requests[index].fp));
        fflush(stdout);
//     }
}

/* return 1 if contain prompt else return 0 */
int contain_prompt() {
    int i;
    for(i=0 ; i < strlen(buf) ; i++) {
        if(buf[i] == '%')   return 1;
    }
    return 0;
}

/* read a line from file and then write it to remote host */
void write_command_next(int index) {

    int n;

    /* read a line from file */
    if(!fgets(buf, BUFSIZE, requests[index].fp)) {
        log_err("fgets");
    }
    write_content_at(index, 'm', wrap_html(buf), 0);

    if(buf[0] == '\n')  return;
    if (DEBUG)
        fprintf(stderr, "%s[%d]\n", buf, (int)strlen(buf));
    n = write(requests[index].socket, buf, strlen(buf));
    if(n < 0)   log_err("error writing to socket");
}

/* serve all the connection established in setup phase */
void serve_connection()
{
    int i, sockfd, max_s, n;
    int activity;
    struct timeval timeout; // second, microsecs
    fd_set fds;

    timeout.tv_sec = 100;
    timeout.tv_usec = 0;

    while(1) {

        FD_ZERO(&fds);
        max_s = 0;

        /* set connected socket to fds */
        for (i = 0; i < REQUEST_MAX_NUM; i++) {
            sockfd = requests[i].socket;
            if(!sockfd) continue;
            if(sockfd > max_s) max_s = sockfd;

            FD_SET(sockfd, &fds);
            if (DEBUG)
                fprintf(stderr, "socket[%d]=%d is set\n", i+1, sockfd);
        }

        if (!max_s) {
            fprintf(stderr, "no more existing socket\n");
            break;
        }

        /* use select to monitor connections */
        activity = select(max_s+1, &fds, NULL, NULL, &timeout);
        if( (activity < 0) && (errno!=EINTR) ) {
            log_err("select");
        } else if( activity == 0 ) {
            fprintf(stderr, "timeout\n");
            break;
        }

        for (i = 0; i < REQUEST_MAX_NUM; i++) {

            sockfd = requests[i].socket;
            if (!sockfd) continue;
            if (!FD_ISSET(sockfd, &fds)) continue;

            /* read from sockfd */
            bzero(buf, BUFSIZE);
            n = read(sockfd, buf, BUFSIZE);

            /* close */
            if (n <= 0) {
                fprintf(stderr, "close socket[%d]=%d\n", i+1, sockfd);
                close(sockfd);
                requests[i].socket = 0;
                fclose(requests[i].fp);
                FD_CLR(sockfd, &fds);
            }
            write_content_at(i, 'm', wrap_html(buf), 0);

            /* if prompt arrive then write command from file to remote host */
            if (contain_prompt()) {

                write_command_next(i);
            }
        }
    }
}

void clients_handler()
{
    int i = 0;

    for(i = 0; i < REQUEST_MAX_NUM; i++) {
        Request r = requests[i];
        /* if one of ip/port/filename is empty then forget it */
        if (!(r.ip && r.port && r.filename)) {
            log_warn("request[%d] ip=%s port=%s filename=%s\n",
                    i, r.ip, r.port, r.filename);
            continue;
        }

        setup_connection(i);
    }

    serve_connection();
}
