#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cdss/stack.h>

#define MAX_REQUEST_LEN 20000000 // 20m
#define FIRST_BUFFER_SIZE 1000 // 1k

enum http_status { HTTP_WAIT_FOR_REQUEST_HEADER, HTTP_SHUTDOWN };
struct http_state {
    enum http_status   state;
    struct sockaddr_in client;
    char *             recv_buf;
    size_t             recv_buf_size;
    size_t             recv_buf_read_index;
    size_t             parse_index;
};

int
espresso_listen(int port)
{
    struct sockaddr_in serv_addr = {0};
    int                fd;

    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port        = htons(port);

    memset(&serv_addr.sin_zero, 0, sizeof(serv_addr.sin_zero));

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to open socket.\n");
        abort();
    }

    bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    listen(fd, 16);

    fcntl(fd, F_SETFL, O_NONBLOCK);

    return fd;
}

static void
init_http_state(struct http_state *state)
{
    state->state               = HTTP_WAIT_FOR_REQUEST_HEADER;
    state->recv_buf            = (char *) malloc(FIRST_BUFFER_SIZE);
    state->recv_buf_size       = FIRST_BUFFER_SIZE;
    state->recv_buf_read_index = 0;
    state->parse_index         = 0;
}

static void
connection_continue_reading(struct http_state *state, int fd)
{
    int again = 0;
    do
    {
        ssize_t avail = state->recv_buf_size - state->recv_buf_read_index;
        ssize_t num_read;

        if ((num_read = read(fd, state->recv_buf + state->recv_buf_read_index, avail)) == avail)
        {
            size_t new_size = state->recv_buf_size * 2;

            if (new_size >= MAX_REQUEST_LEN)
            {
                state->state = HTTP_SHUTDOWN;
                break;
            }

            printf("doingd it %li \t %li\n", new_size, state->recv_buf_size);
            fflush(stdout);
            char *new_buf = realloc(state->recv_buf, new_size);
            printf("did it \n");
            fflush(stdout);

            if (new_buf)
            {
                again                = 1;
                state->recv_buf      = new_buf;
                state->recv_buf_size = new_size;
            }
        }
        else
        {
            again = 0;
            state->recv_buf_read_index += num_read;
        }
    } while (again);
}

void
espresso_handle_connection(struct http_state *http_state, struct pollfd *fd)
{
    if (fd->revents & POLLIN) { connection_continue_reading(http_state, fd->fd); }

    switch (http_state->state)
    {
    case HTTP_WAIT_FOR_REQUEST_HEADER:
        while (http_state->parse_index < http_state->recv_buf_read_index)
            printf("%c", http_state->recv_buf[http_state->parse_index++]);
        break;
    case HTTP_SHUTDOWN:
        if (http_state->recv_buf) free(http_state->recv_buf);
        shutdown(fd->fd, 2);
        break;
    }

    fd->revents = 0; // reset for next exent
}

int
main()
{
    stack_t *connections_http;
    stack_t *connections_poll;
    int      socket;

    socket           = espresso_listen(8080);
    connections_http = stack_create(sizeof(struct http_state), 512, 2.0);
    connections_poll = stack_create(sizeof(struct pollfd), 512, 2.0);

    /*
     * Main loop: (2 parts)
     *   1. Accept any new connections
     *   2. use poll() to check for new input
     */
    while (1)
    {
        socklen_t         clen = sizeof(struct sockaddr_in);
        struct http_state new_client_http;
        struct pollfd     new_client_poll;
        while ((new_client_poll.fd = accept(socket, (struct sockaddr *) &new_client_http.client, &clen)) > 0)
        {
            new_client_poll.events  = POLLIN;
            new_client_poll.revents = 0;

            char addrstr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(new_client_http.client.sin_addr), addrstr, INET_ADDRSTRLEN);
            printf("[REQUEST] from %s:%i\n", addrstr, ntohs(new_client_http.client.sin_port));

            init_http_state(&new_client_http);

            stack_push(connections_http, &new_client_http);
            stack_push(connections_poll, &new_client_poll);
        }

        int                num_connections       = stack_objects_get_num(connections_http);
        struct http_state *connections_http_data = stack_element_ref(connections_http, 0);
        struct pollfd *    connections_poll_data = stack_element_ref(connections_poll, 0);

        poll(stack_element_ref(connections_poll, 0), num_connections, 10); // 10ms

        for (int connection = 0; connection < num_connections; connection++)
            if (connections_poll_data[connection].fd >= 1)
                espresso_handle_connection(&connections_http_data[connection], &connections_poll_data[connection]);
    }
}
