#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct c_client {
    int         id;
    int         fd;
    char        *buff;
    char        *recv_str;
    struct c_client    *next;
} t_client;

t_client    *lst;
int         sockfd;
int         max_fd;
int         id;
int         problem;
fd_set      fds1;
fd_set      fds2;

void putstr_fd( int fd, const char *str ) {
    if ( str == NULL ) { return; }
    write(fd, str, strlen(str) + 1);
}

void    clean_all( void ) {
    while(lst) {
        t_client *temp = lst->next;
        close(lst->fd);
        if (lst->buff)
            free(lst->buff);
        free(lst);
        lst = temp;
    }
    if (sockfd > 0)
        close(sockfd);
}

void    init_set() {
    FD_ZERO(&fds1);
    FD_ZERO(&fds2);
    max_fd = 0;
    for (t_client *temp = lst; temp; temp = temp->next) {
        if (temp->fd > max_fd)
            max_fd = temp->fd;
        FD_SET(temp->fd, &fds1);
        FD_SET(temp->fd, &fds2);
    }
    for (t_client *temp = lst; temp; temp = temp->next) {
        if (FD_ISSET(temp->fd, &fds1) == 0) {
            problem = 1;
            break ;
        }
    }            
}

void    flush_buff( t_client *client ) {
    if (!client->buff) 
        return ;
    int bytes_to_send = strlen(client->buff);
    char *str = client->buff;
    while (bytes_to_send > 0) {
        int retval = send(client->fd, client->buff, bytes_to_send, 0);
        if (retval == -1) {
            problem = 1;
            return ;
        }
        client->buff += retval;
        bytes_to_send -= retval;
    }
    client->buff = str;
    free(client->buff);
    client->buff = NULL;
}

void    add_to_buff( t_client *dest, char *str) {
    if (!dest || !str) { return ; }
    if (!dest->buff) {
        dest->buff = calloc(strlen(str) + 1, 1);
        if (!dest->buff) {
            problem = 1;
            return ;
        }
        strcpy(dest->buff, str);
    }
    else {
        char *new_buff = realloc(dest->buff, strlen(dest->buff) + strlen(str) + 1);
        if (!new_buff) {
            problem = 1;
            return ;
        }
        dest->buff = new_buff;
        strcat(dest->buff, str);
    }
}

void    add_to_all( int client_id, char *str) {
    if (str == NULL)
        return ;
    for (t_client *temp = lst; temp; temp = temp->next) {
        if (temp->id != client_id)
            add_to_buff(temp, str);
    }
}

void    accept_connection() {
    struct sockaddr test;
    unsigned int len = sizeof(test);
    int new_fd = accept(lst->fd, &test, &len);
    if (new_fd == -1) {
        problem = 1;
        return ;
    }
    t_client *temp = lst;
    while(temp->next)
        temp = temp->next;
    t_client *new_client = calloc(sizeof(t_client), 1);
    if (!new_client) {
        close (new_fd);
        problem = 1;
        return ;
    }
    new_client->id = id;
    id++;
    new_client->fd = new_fd;
    temp->next = new_client;
    char temp_str[1024];
    bzero(temp_str, 1024);
    sprintf(temp_str, "server: client %d just arrived\n", new_client->id);
    // printf("debug: [%s]\n", temp_str);
    add_to_all(new_client->id, temp_str);
}

void    client_quit( t_client *client ) {
    char temp_str[1024];
    bzero(temp_str, sizeof temp_str);
    sprintf(temp_str, "server: client %d just left\n", client->id);
    // printf("debug [%s]\n", temp_str);
    add_to_all(client->id, temp_str);
    if (problem)
        return ;
    t_client *temp = lst;
    while (1) {
        if (temp->next == client)
            break ;
        temp = temp->next;
    }
    t_client *tmp_cl = temp->next;
    temp->next = temp->next->next;
    close(tmp_cl->fd);
    if (tmp_cl->buff)
        free(tmp_cl->buff);
    free(tmp_cl);
}

void send_msg( t_client *client, char *str ) {
    while (1) {
        char *it = strstr(str, "\n");
        if (!it)
            break ;
        size_t len = 0;
        char *temp = str;
        while (temp != it) {
            temp++;
            len++;
        }
        char *temp2 = calloc(len + 1, 1);
        if (!temp2) {
            problem = 1;
            return ;
        }
        for ( size_t i = 0; i < len; i++)
            temp2[i] = str[i];
        char *result = calloc(30, 1);
        if (!result) {
            problem = 1;
            free(temp2);
            return ;
        }
        sprintf(result, "client %d: ", client->id);
        result = realloc(result, strlen(result) + strlen(temp2) + 2);
        if (!result) {
            free(result);
            free(temp2);
            problem = 1;
            return ;
        }
        strcat(result, temp2);
        strcat(result, "\n");
        // printf("debug: [%s]\n", result);
        add_to_all(client->id, result);
        if (problem)
            break ;
        str += (len + 1);
    }
}

int    accept_input( t_client *client ) {
    char result[4096];
    bzero(result, sizeof result);
    int retval = recv(client->fd, result, 4096, 0);
    if (retval == -1) {
        problem = 1;
        return 0;
    }
    if (retval == 0) {
        client_quit(client);
        return 1;
    }
    // printf("debug: [%s]\n", result);
    send_msg(client, result);
    return (0);
}

void routine( void ) {
    lst = calloc(sizeof(t_client), 1);
    if (!lst)
        return ;
    lst->id = -1;
    lst->fd = sockfd;
    while(!problem) {
        init_set();
        int retval = select(max_fd + 1, &fds1, &fds2, NULL, NULL);
        if (retval == -1)
            return ;
        for ( t_client *temp = lst; temp; temp = temp->next ) {
            if (FD_ISSET(temp->fd, &fds1)) {
                if (temp->fd == sockfd) {
                    accept_connection();
                    if (problem)
                        return ;
                }
                else {
                    if (accept_input(temp))
                        break ;
                }
            }
            if (problem)
                break ;
        }
        for ( t_client *temp = lst; temp; temp = temp->next ) {
             if (FD_ISSET(temp->fd, &fds2))
                flush_buff(temp);
        }
    }
}

int main( int ac, char **av ) {
    if (ac != 2) {
        return (putstr_fd(2, "Wrong number of arguments\n"), 1);
    }
    //initiate socket
    memset(&lst, 0, (void *)&fds2 - (void *)&lst);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        return (putstr_fd(2, "Fatal error\n"), 1);
    struct sockaddr_in sock_opt;
    bzero(&sock_opt, sizeof sock_opt);
    sock_opt.sin_family = AF_INET;
    sock_opt.sin_addr.s_addr = htonl(2130706433); //listen to 127.0.0.1
    sock_opt.sin_port = htons(atoi(av[1]));
    if (bind(sockfd, (struct sockaddr *)(&sock_opt), sizeof(sock_opt)) == -1)
        return (clean_all(), putstr_fd(2, "Fatal error\n"), 1);
    if (listen(sockfd, SOMAXCONN) == -1)
        return (clean_all(), putstr_fd(2, "Fatal error\n"), 1);
    
    routine();
    putstr_fd(2, "Fatal error\n");
    clean_all();
    return ( 0);
}