#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

typedef struct s_client {
    int     id;
    int     used;
    char    *msg;
} t_client;

t_client    clients[1024];

void    ft_error( void ) {
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    for ( size_t i = 0; i < 1024; i++ ) {
        if (clients[i].used)
            close(i);
        if (clients[i].msg)
            free(clients[i].msg);
    }
    exit(1);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				ft_error();
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == NULL)
		ft_error();
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void	ft_message( int client, int mode, char *str ) {
	char to_send[64 + strlen(str)];
	bzero(to_send, 64 + strlen(str));
	switch (mode) {
		case 1:  {
			sprintf(to_send, "server: client %d just arrived\n", clients[client].id);
			break ;
		}
		case 2: {
			sprintf(to_send, "server: client %d just left\n", clients[client].id);
		}
		default: {
			sprintf(to_send, "client %d: %s", clients[client].id, str);
		}
	}
	for ( int i = 0; i < 1024; i++ ) {
		if (i != client && clients[i].used && clients[i].id != -1) {
			send(i, to_send, strlen(to_send), 0);
		}
	}
}


int main( int ac, char **av ) {
    int     sockfd;
    int     id_use;
    struct  sockaddr_in servaddr;
	fd_set 	base, setcopy;
	char	buff[4096];
	char	*msg;

    if (ac != 2) {
        return (write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n")), 1);
	}
	sockfd = 0;
	id_use = 0;
	bzero(&servaddr, sizeof (servaddr));
	FD_ZERO(&base);
	FD_ZERO(&setcopy);
	msg = NULL;
    sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd == -1)
        ft_error();
    servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));
    if (bind(sockfd, (struct sockaddr *)(&servaddr), sizeof (servaddr)) == -1)
        ft_error();
    if (listen(sockfd, 10) == -1)
        ft_error();
    FD_SET(sockfd, &base);
	clients[sockfd].id = -1;
	clients[sockfd].used = 1;
    while(1) {
		setcopy = base;
		if (select(1024, &setcopy, NULL, NULL, NULL) == -1)
			ft_error();
		for ( int i = 0; i < 1024; i++ ) {
			if (clients[i].used == 0 || !FD_ISSET(i, &setcopy))
				continue ;
			if (i == sockfd) {
				int new = accept(sockfd, NULL, NULL);
				if (new == -1)
					ft_error();
				FD_SET(new, &base);
				clients[new].used = 1;
				clients[new].id = id_use;
				id_use++;
				ft_message(new, 1, "");
				continue ;
			}
			bzero(buff, 4096);
			if (recv(i, buff, 4095, 0) == 0) {
				ft_message(i, 2, "");
				FD_CLR(i, &base);
				close(i);
				clients[i].used = 0;
				if (clients[i].msg)
					free(clients[i].msg);
				clients[i].msg = NULL;
				continue ;
			}
			clients[i].msg = str_join(clients[i].msg, buff);
			while (extract_message(&clients[i].msg, &msg)) {
				ft_message(i, 0, msg);
				free(msg);
				msg = NULL;
			}
		}
    }
    return (0);
}