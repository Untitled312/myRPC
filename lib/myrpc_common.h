#ifndef MYRPC_COMMON_H
#define MYRPC_COMMON_H

#include <stddef.h>

#define MYRPC_VERSION       "1.0"
#define MYRPC_CONF_PATH     "/etc/myrpc/myrpc.conf"
#define MYRPC_USERS_PATH    "/etc/myrpc/users.conf"
#define MYRPC_LOG_PATH      "/var/log/myrpc.log"
#define MYRPC_RUN_DIR       "/run/myrpc-server"
#define MYRPC_PID_FILE      "/run/myrpc-server/myrpc-server.pid"
#define MYRPC_DEFAULT_PORT  5000
#define MYRPC_BUF_SIZE      65536
#define MYRPC_MAX_USERS     256
#define MYRPC_MAX_USER_LEN  64
#define MYRPC_MAX_CMD_LEN   4096

enum myrpc_sock_type
{
  MYRPC_SOCK_STREAM = 0,
  MYRPC_SOCK_DGRAM  = 1
};

enum myrpc_code
{
  MYRPC_OK    = 0,
  MYRPC_ERROR = 1
};

struct myrpc_request
{
  char login[MYRPC_MAX_USER_LEN];
  char command[MYRPC_MAX_CMD_LEN];
};

struct myrpc_response
{
  int code;
  char result[MYRPC_BUF_SIZE];
};

struct myrpc_config
{
  int port;
  enum myrpc_sock_type sock_type;
};

int myrpc_parse_request (const char *json, struct myrpc_request *req);
int myrpc_parse_response (const char *json, struct myrpc_response *resp);
size_t myrpc_encode_request (const struct myrpc_request *req,
                             char *buf, size_t buf_size);
size_t myrpc_encode_response (const struct myrpc_response *resp,
                              char *buf, size_t buf_size);
size_t myrpc_json_escape (const char *src, char *dst, size_t dst_size);
int myrpc_read_config (const char *path, struct myrpc_config *cfg);
int myrpc_user_allowed (const char *path, const char *login);

#endif