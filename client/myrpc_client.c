#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "myrpc_common.h"

static void
usage (const char *prog)
{
  fprintf (stdout,
           "Usage: %s [OPTIONS]\n"
           "  -c, --command CMD   команда BASH для выполнения\n"
           "  -h, --host ADDR     адрес сервера\n"
           "  -p, --port PORT     порт сервера (по умолчанию %d)\n"
           "  -s, --stream        потоковый сокет (TCP)\n"
           "  -d, --dgram         датаграмный сокет (UDP)\n"
           "      --help          показать эту справку\n",
           prog, MYRPC_DEFAULT_PORT);
}

int
main (int argc, char **argv)
{
  const char *command = NULL;
  const char *host = NULL;
  int port = MYRPC_DEFAULT_PORT;
  enum myrpc_sock_type stype = MYRPC_SOCK_STREAM;

  static struct option long_options[] =
    {
      { "command", required_argument, 0, 'c' },
      { "host",    required_argument, 0, 'h' },
      { "port",    required_argument, 0, 'p' },
      { "stream",  no_argument,       0, 's' },
      { "dgram",   no_argument,       0, 'd' },
      { "help",    no_argument,       0, 'H' },
      { 0, 0, 0, 0 }
    };

  int opt;
  while ((opt = getopt_long (argc, argv, "c:h:p:sd",
                             long_options, NULL)) != -1)
    {
      switch (opt)
        {
        case 'c': command = optarg; break;
        case 'h': host    = optarg; break;
        case 'p': port    = atoi (optarg); break;
        case 's': stype   = MYRPC_SOCK_STREAM; break;
        case 'd': stype   = MYRPC_SOCK_DGRAM;  break;
        case 'H': usage (argv[0]); return 0;
        default:  usage (argv[0]); return 2;
        }
    }

  if (!command || !host)
    {
      fprintf (stderr, "myrpc-client: обязательные опции: -c и -h\n");
      usage (argv[0]);
      return 2;
    }

  const char *login = getenv ("USER");
  if (!login)
    login = "unknown";

  struct myrpc_request req;
  strncpy (req.login, login, MYRPC_MAX_USER_LEN - 1);
  req.login[MYRPC_MAX_USER_LEN - 1] = '\0';
  strncpy (req.command, command, MYRPC_MAX_CMD_LEN - 1);
  req.command[MYRPC_MAX_CMD_LEN - 1] = '\0';

  char buf[MYRPC_BUF_SIZE * 2];
  size_t len = myrpc_encode_request (&req, buf, sizeof buf);

  int sock_type = (stype == MYRPC_SOCK_STREAM) ? SOCK_STREAM : SOCK_DGRAM;
  int fd = socket (AF_INET, sock_type, 0);
  if (fd < 0)
    {
      perror ("myrpc-client: socket");
      return 1;
    }

  struct sockaddr_in addr;
  memset (&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons ((uint16_t)port);
  if (inet_pton (AF_INET, host, &addr.sin_addr) != 1)
    {
      struct hostent *he = gethostbyname (host);
      if (!he)
        {
          fprintf (stderr, "myrpc-client: не удалось разрешить %s\n", host);
          close (fd);
          return 1;
        }
      memcpy (&addr.sin_addr, he->h_addr, (size_t)he->h_length);
    }

  if (stype == MYRPC_SOCK_STREAM)
    {
      if (connect (fd, (struct sockaddr *)&addr, sizeof addr) < 0)
        {
          perror ("myrpc-client: connect");
          close (fd);
          return 1;
        }
      if (send (fd, buf, len, 0) < 0)
        {
          perror ("myrpc-client: send");
          close (fd);
          return 1;
        }
      ssize_t n = recv (fd, buf, sizeof buf - 1, 0);
      if (n <= 0)
        {
          perror ("myrpc-client: recv");
          close (fd);
          return 1;
        }
      buf[n] = '\0';
    }
  else
    {
      if (sendto (fd, buf, len, 0,
                  (struct sockaddr *)&addr, sizeof addr) < 0)
        {
          perror ("myrpc-client: sendto");
          close (fd);
          return 1;
        }
      ssize_t n = recvfrom (fd, buf, sizeof buf - 1, 0, NULL, NULL);
      if (n <= 0)
        {
          perror ("myrpc-client: recvfrom");
          close (fd);
          return 1;
        }
      buf[n] = '\0';
    }

  close (fd);

  struct myrpc_response resp;
  if (myrpc_parse_response (buf, &resp) != 0)
    {
      fprintf (stderr, "myrpc-client: некорректный ответ сервера\n");
      return 1;
    }

  if (resp.code == MYRPC_OK)
    {
      fputs (resp.result, stdout);
      return 0;
    }
  else
    {
      fprintf (stderr, "%s", resp.result);
      return 1;
    }
}