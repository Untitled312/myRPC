#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "myrpc_common.h"

static volatile sig_atomic_t g_terminate = 0;
static volatile sig_atomic_t g_reload    = 0;
static volatile sig_atomic_t g_child     = 0;

static struct myrpc_config g_config;
static int g_foreground = 0;
static const char *g_log_file = NULL;
static FILE *g_log_fp = NULL;

static void
log_msg (const char *fmt, ...)
{
  va_list ap;
  if (g_log_fp)
    {
      va_start (ap, fmt);
      vfprintf (g_log_fp, fmt, ap);
      va_end (ap);
      fputc ('\n', g_log_fp);
      fflush (g_log_fp);
    }
  else
    {
      va_start (ap, fmt);
      vsyslog (LOG_INFO, fmt, ap);
      va_end (ap);
    }
}

static void
on_term (int sig)
{
  (void)sig;
  g_terminate = 1;
}

static void
on_hup (int sig)
{
  (void)sig;
  g_reload = 1;
}

static void
on_child (int sig)
{
  (void)sig;
  g_child = 1;
}

static void
install_signals (void)
{
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  sa.sa_handler = on_term;
  sigaction (SIGTERM, &sa, NULL);
  sigaction (SIGINT,  &sa, NULL);

  sa.sa_handler = on_hup;
  sigaction (SIGHUP, &sa, NULL);

  sa.sa_handler = on_child;
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction (SIGCHLD, &sa, NULL);
}

static void
daemonize (void)
{
  pid_t pid = fork ();
  if (pid < 0)
    {
      perror ("fork");
      exit (1);
    }
  if (pid > 0)
    exit (0);

  if (setsid () < 0)
    {
      perror ("setsid");
      exit (1);
    }

  pid = fork ();
  if (pid < 0)
    exit (1);
  if (pid > 0)
    exit (0);

  umask (0027);
  if (chdir ("/") != 0)
    {
    }
  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);
  int fd = open ("/dev/null", O_RDWR);
  if (fd >= 0)
    {
      dup2 (fd, STDIN_FILENO);
      dup2 (fd, STDOUT_FILENO);
      dup2 (fd, STDERR_FILENO);
      if (fd > 2) close (fd);
    }
}

static void
write_pidfile (void)
{
  if (access (MYRPC_RUN_DIR, F_OK) != 0)
    {
      if (mkdir (MYRPC_RUN_DIR, 0755) != 0 && errno != EEXIST)
        log_msg ("не удалось создать %s: %s",
                 MYRPC_RUN_DIR, strerror (errno));
    }

  FILE *f = fopen (MYRPC_PID_FILE, "w");
  if (f)
    {
      fprintf (f, "%d\n", getpid ());
      fclose (f);
    }
  else
    log_msg ("не удалось записать PID-файл %s: %s",
             MYRPC_PID_FILE, strerror (errno));
}

static void
remove_pidfile (void)
{
  unlink (MYRPC_PID_FILE);
}

static int
drop_privileges (void)
{
  struct passwd *pw = getpwnam ("myrpc");
  if (!pw)
    {
      log_msg ("пользователь myrpc не найден, работаю от root");
      return -1;
    }

  if (initgroups ("myrpc", pw->pw_gid) != 0)
    {
      log_msg ("initgroups: %s", strerror (errno));
      return -1;
    }
  if (setgid (pw->pw_gid) != 0)
    {
      log_msg ("setgid: %s", strerror (errno));
      return -1;
    }
  if (setuid (pw->pw_uid) != 0)
    {
      log_msg ("setuid: %s", strerror (errno));
      return -1;
    }

  log_msg ("понижены привилегии до пользователя myrpc (uid=%d)",
           (int)pw->pw_uid);
  return 0;
}

static int
run_command (const char *command, struct myrpc_response *resp)
{
  char out_tmpl[] = "/tmp/myRPC_XXXXXX";
  char err_tmpl[] = "/tmp/myRPC_XXXXXX";
  int out_fd = mkstemp (out_tmpl);
  int err_fd = mkstemp (err_tmpl);
  if (out_fd < 0 || err_fd < 0)
    {
      if (out_fd >= 0) close (out_fd);
      if (err_fd >= 0) close (err_fd);
      resp->code = MYRPC_ERROR;
      snprintf (resp->result, sizeof resp->result,
                "не удалось создать временные файлы");
      return -1;
    }

  pid_t pid = fork ();
  if (pid < 0)
    {
      close (out_fd); close (err_fd);
      unlink (out_tmpl); unlink (err_tmpl);
      resp->code = MYRPC_ERROR;
      snprintf (resp->result, sizeof resp->result, "fork: %s",
                strerror (errno));
      return -1;
    }

  if (pid == 0)
    {
      dup2 (out_fd, STDOUT_FILENO);
      dup2 (err_fd, STDERR_FILENO);
      close (out_fd);
      close (err_fd);
      execl ("/bin/sh", "sh", "-c", command, (char *)NULL);
      _exit (127);
    }

  close (out_fd);
  close (err_fd);

  int status = 0;
  while (waitpid (pid, &status, 0) < 0)
    if (errno != EINTR) break;

  FILE *fo = fopen (out_tmpl, "r");
  if (fo)
    {
      size_t n = fread (resp->result, 1, sizeof resp->result - 1, fo);
      resp->result[n] = '\0';
      fclose (fo);
    }

  if (WIFEXITED (status) && WEXITSTATUS (status) == 0)
    resp->code = MYRPC_OK;
  else
    {
      resp->code = MYRPC_ERROR;
      FILE *fe = fopen (err_tmpl, "r");
      if (fe)
        {
          size_t n = fread (resp->result, 1, sizeof resp->result - 1, fe);
          resp->result[n] = '\0';
          fclose (fe);
        }
      if (resp->result[0] == '\0')
        snprintf (resp->result, sizeof resp->result,
                  "команда завершилась с кодом %d",
                  WIFEXITED (status) ? WEXITSTATUS (status) : -1);
    }

  unlink (out_tmpl);
  unlink (err_tmpl);
  return 0;
}

static void
handle_request (const char *buf, ssize_t n,
                struct sockaddr_in *peer, int fd, int is_stream)
{
  if (n <= 0)
    return;

  char req_buf[MYRPC_BUF_SIZE * 2];
  if ((size_t)n >= sizeof req_buf)
    n = (ssize_t)(sizeof req_buf - 1);
  memcpy (req_buf, buf, (size_t)n);
  req_buf[n] = '\0';

  struct myrpc_request req;
  if (myrpc_parse_request (req_buf, &req) != 0)
    {
      log_msg ("некорректный запрос от %s", inet_ntoa (peer->sin_addr));
      return;
    }

  log_msg ("запрос от %s@%s: %.64s",
           req.login, inet_ntoa (peer->sin_addr), req.command);

  struct myrpc_response resp;
  memset (&resp, 0, sizeof resp);

  if (!myrpc_user_allowed (MYRPC_USERS_PATH, req.login))
    {
      resp.code = MYRPC_ERROR;
      snprintf (resp.result, sizeof resp.result,
                "пользователь '%s' не разрешён", req.login);
      log_msg ("отказано пользователю %s", req.login);
    }
  else
    {
      run_command (req.command, &resp);
    }

  char ans[MYRPC_BUF_SIZE * 2];
  size_t alen = myrpc_encode_response (&resp, ans, sizeof ans);

  if (is_stream)
    send (fd, ans, alen, 0);
  else
    sendto (fd, ans, alen, 0,
            (struct sockaddr *)peer, sizeof *peer);
}

static void
serve_stream (int lfd)
{
  while (!g_terminate)
    {
      if (g_reload)
        {
          g_reload = 0;
          if (myrpc_read_config (MYRPC_CONF_PATH, &g_config) == 0)
            log_msg ("конфигурация перезагружена");
        }
      if (g_child)
        {
          g_child = 0;
          int st;
          pid_t p;
          while ((p = waitpid (-1, &st, WNOHANG)) > 0)
            log_msg ("дочерний процесс %d завершился, статус %d",
                     (int)p, WEXITSTATUS (st));
        }

      struct sockaddr_in peer;
      socklen_t plen = sizeof peer;
      int cfd = accept (lfd, (struct sockaddr *)&peer, &plen);
      if (cfd < 0)
        {
          if (errno == EINTR) continue;
          log_msg ("accept: %s", strerror (errno));
          continue;
        }

      pid_t pid = fork ();
      if (pid < 0)
        {
          log_msg ("fork: %s", strerror (errno));
          close (cfd);
          continue;
        }
      if (pid == 0)
        {
          close (lfd);
          char buf[MYRPC_BUF_SIZE * 2];
          ssize_t n = recv (cfd, buf, sizeof buf - 1, 0);
          if (n > 0)
            handle_request (buf, n, &peer, cfd, 1);
          close (cfd);
          _exit (0);
        }
      close (cfd);
    }
}

static void
serve_dgram (int fd)
{
  char buf[MYRPC_BUF_SIZE * 2];
  while (!g_terminate)
    {
      if (g_reload)
        {
          g_reload = 0;
          if (myrpc_read_config (MYRPC_CONF_PATH, &g_config) == 0)
            log_msg ("конфигурация перезагружена");
        }
      if (g_child)
        {
          g_child = 0;
          int st; pid_t p;
          while ((p = waitpid (-1, &st, WNOHANG)) > 0)
            log_msg ("дочерний процесс %d завершился", (int)p);
        }

      struct sockaddr_in peer;
      socklen_t plen = sizeof peer;
      ssize_t n = recvfrom (fd, buf, sizeof buf - 1, 0,
                            (struct sockaddr *)&peer, &plen);
      if (n < 0)
        {
          if (errno == EINTR) continue;
          log_msg ("recvfrom: %s", strerror (errno));
          continue;
        }
      handle_request (buf, n, &peer, fd, 0);
    }
}

static void
usage (const char *prog)
{
  fprintf (stderr,
           "Usage: %s [OPTIONS]\n"
           "  -f, --foreground   не переходить в режим демона\n"
           "  -l, --log FILE     писать журнал в FILE вместо syslog\n"
           "      --help         показать эту справку\n",
           prog);
}

int
main (int argc, char **argv)
{
  static struct option long_options[] =
    {
      { "foreground", no_argument,       0, 'f' },
      { "log",        required_argument, 0, 'l' },
      { "help",       no_argument,       0, 'H' },
      { 0, 0, 0, 0 }
    };

  int opt;
  while ((opt = getopt_long (argc, argv, "fl:",
                             long_options, NULL)) != -1)
    {
      switch (opt)
        {
        case 'f': g_foreground = 1; break;
        case 'l': g_log_file   = optarg; break;
        case 'H': usage (argv[0]); return 0;
        default:  usage (argv[0]); return 2;
        }
    }

  if (myrpc_read_config (MYRPC_CONF_PATH, &g_config) != 0)
    fprintf (stderr,
             "myrpc-server: не удалось прочитать %s, "
             "используются значения по умолчанию\n",
             MYRPC_CONF_PATH);

  if (g_log_file)
    {
      g_log_fp = fopen (g_log_file, "a");
      if (!g_log_fp)
        {
          perror ("open log");
          return 1;
        }
    }
  else
    {
      openlog ("myrpc-server", LOG_PID, LOG_DAEMON);
    }

  if (!g_foreground)
    daemonize ();

  install_signals ();
  write_pidfile ();
  log_msg ("myrpc-server %s запущен, порт %d, тип %s",
           MYRPC_VERSION, g_config.port,
           g_config.sock_type == MYRPC_SOCK_STREAM ? "stream" : "dgram");

  int sock_type = (g_config.sock_type == MYRPC_SOCK_STREAM)
                  ? SOCK_STREAM : SOCK_DGRAM;
  int fd = socket (AF_INET, sock_type, 0);
  if (fd < 0)
    {
      log_msg ("socket: %s", strerror (errno));
      return 1;
    }

  int reuse = 1;
  setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

  struct sockaddr_in addr;
  memset (&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl (INADDR_ANY);
  addr.sin_port = htons ((uint16_t)g_config.port);

  if (bind (fd, (struct sockaddr *)&addr, sizeof addr) < 0)
    {
      log_msg ("bind: %s", strerror (errno));
      close (fd);
      return 1;
    }

  if (g_config.sock_type == MYRPC_SOCK_STREAM)
    {
      if (listen (fd, 16) < 0)
        {
          log_msg ("listen: %s", strerror (errno));
          close (fd);
          return 1;
        }
    }

  if (!g_foreground)
    drop_privileges ();

  if (g_config.sock_type == MYRPC_SOCK_STREAM)
    serve_stream (fd);
  else
    serve_dgram (fd);

  close (fd);
  remove_pidfile ();
  log_msg ("myrpc-server остановлен");
  if (g_log_fp) fclose (g_log_fp);
  else closelog ();
  return 0;
}