#include "myrpc_common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t
myrpc_json_escape (const char *src, char *dst, size_t dst_size)
{
  size_t si = 0, di = 0;
  int overflow = 0;

  if (!src || !dst || dst_size == 0)
    return 0;

  while (src[si] != '\0' && !overflow)
    {
      unsigned char c = (unsigned char)src[si++];
      switch (c)
        {
        case '"':
        case '\\':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
          if (di + 2 >= dst_size)
            { overflow = 1; break; }
          dst[di++] = '\\';
          switch (c)
            {
            case '"':  dst[di++] = '"';  break;
            case '\\': dst[di++] = '\\'; break;
            case '\b': dst[di++] = 'b';  break;
            case '\f': dst[di++] = 'f';  break;
            case '\n': dst[di++] = 'n';  break;
            case '\r': dst[di++] = 'r';  break;
            case '\t': dst[di++] = 't';  break;
            default: break;
            }
          break;

        default:
          if (c < 0x20)
            {
              if (di + 6 >= dst_size)
                { overflow = 1; break; }
              di += (size_t)snprintf (dst + di, dst_size - di,
                                      "\\u%04x", c);
            }
          else
            {
              if (di + 1 >= dst_size)
                { overflow = 1; break; }
              dst[di++] = (char)c;
            }
          break;
        }
    }

  dst[di] = '\0';
  return di;
}

static const char *
skip_ws (const char *p)
{
  while (*p && isspace ((unsigned char)*p))
    p++;
  return p;
}

static const char *
read_json_string (const char *p, char *out, size_t out_size)
{
  size_t oi = 0;
  if (*p != '"')
    return NULL;
  p++;
  while (*p && *p != '"')
    {
      if (*p == '\\' && p[1] != '\0')
        {
          p++;
          char c;
          switch (*p)
            {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '/':  c = '/';  break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            default:   c = *p;   break;
            }
          if (oi + 1 < out_size)
            out[oi++] = c;
          p++;
        }
      else
        {
          if (oi + 1 < out_size)
            out[oi++] = *p;
          p++;
        }
    }
  if (*p != '"')
    return NULL;
  out[oi] = '\0';
  return p + 1;
}

int
myrpc_parse_request (const char *json, struct myrpc_request *req)
{
  if (!json || !req)
    return -1;
  req->login[0] = '\0';
  req->command[0] = '\0';

  const char *p = skip_ws (json);
  if (*p != '{')
    return -1;
  p++;

  while (*p && *p != '}')
    {
      p = skip_ws (p);
      if (*p == ',') { p++; continue; }
      if (*p == '}') break;

      char key[64] = { 0 };
      p = read_json_string (p, key, sizeof key);
      if (!p) return -1;
      p = skip_ws (p);
      if (*p != ':') return -1;
      p++;
      p = skip_ws (p);

      char val[MYRPC_MAX_CMD_LEN] = { 0 };
      p = read_json_string (p, val, sizeof val);
      if (!p) return -1;

      if (strcmp (key, "login") == 0)
        {
          strncpy (req->login, val, MYRPC_MAX_USER_LEN - 1);
          req->login[MYRPC_MAX_USER_LEN - 1] = '\0';
        }
      else if (strcmp (key, "command") == 0)
        {
          strncpy (req->command, val, MYRPC_MAX_CMD_LEN - 1);
          req->command[MYRPC_MAX_CMD_LEN - 1] = '\0';
        }
    }

  if (req->login[0] == '\0' || req->command[0] == '\0')
    return -1;
  return 0;
}

int
myrpc_parse_response (const char *json, struct myrpc_response *resp)
{
  if (!json || !resp)
    return -1;
  resp->code = MYRPC_ERROR;
  resp->result[0] = '\0';

  const char *p = skip_ws (json);
  if (*p != '{')
    return -1;
  p++;

  while (*p && *p != '}')
    {
      p = skip_ws (p);
      if (*p == ',') { p++; continue; }
      if (*p == '}') break;

      char key[64] = { 0 };
      p = read_json_string (p, key, sizeof key);
      if (!p) return -1;
      p = skip_ws (p);
      if (*p != ':') return -1;
      p++;
      p = skip_ws (p);

      if (strcmp (key, "code") == 0)
        {
          resp->code = atoi (p);
          while (*p && (isdigit ((unsigned char)*p) || *p == '-'))
            p++;
        }
      else if (strcmp (key, "result") == 0)
        {
          p = read_json_string (p, resp->result, sizeof resp->result);
          if (!p) return -1;
        }
    }
  return 0;
}

size_t
myrpc_encode_request (const struct myrpc_request *req,
                      char *buf, size_t buf_size)
{
  char esc_login[MYRPC_MAX_USER_LEN * 2];
  char esc_cmd[MYRPC_MAX_CMD_LEN * 2];
  myrpc_json_escape (req->login, esc_login, sizeof esc_login);
  myrpc_json_escape (req->command, esc_cmd, sizeof esc_cmd);
  return (size_t)snprintf (buf, buf_size,
                           "{\"login\":\"%s\",\"command\":\"%s\"}",
                           esc_login, esc_cmd);
}

size_t
myrpc_encode_response (const struct myrpc_response *resp,
                       char *buf, size_t buf_size)
{
  char esc[MYRPC_BUF_SIZE * 2];
  myrpc_json_escape (resp->result, esc, sizeof esc);
  return (size_t)snprintf (buf, buf_size,
                           "{\"code\":%d,\"result\":\"%s\"}",
                           resp->code, esc);
}

static char *
trim (char *s)
{
  while (isspace ((unsigned char)*s))
    s++;
  if (*s == '\0')
    return s;
  char *end = s + strlen (s) - 1;
  while (end > s && isspace ((unsigned char)*end))
    *end-- = '\0';
  return s;
}

int
myrpc_read_config (const char *path, struct myrpc_config *cfg)
{
  if (!path || !cfg)
    return -1;
  cfg->port = MYRPC_DEFAULT_PORT;
  cfg->sock_type = MYRPC_SOCK_STREAM;

  FILE *f = fopen (path, "r");
  if (!f)
    return -1;

  char line[256];
  while (fgets (line, sizeof line, f))
    {
      char *s = trim (line);
      if (*s == '\0' || *s == '#')
        continue;
      char *eq = strchr (s, '=');
      if (!eq)
        continue;
      *eq = '\0';
      char *key = trim (s);
      char *val = trim (eq + 1);

      if (strcmp (key, "port") == 0)
        cfg->port = atoi (val);
      else if (strcmp (key, "socket_type") == 0)
        {
          if (strcmp (val, "dgram") == 0)
            cfg->sock_type = MYRPC_SOCK_DGRAM;
          else
            cfg->sock_type = MYRPC_SOCK_STREAM;
        }
    }
  fclose (f);
  return 0;
}

int
myrpc_user_allowed (const char *path, const char *login)
{
  if (!path || !login)
    return 0;
  FILE *f = fopen (path, "r");
  if (!f)
    return 0;
  char line[MYRPC_MAX_USER_LEN];
  while (fgets (line, sizeof line, f))
    {
      char *s = trim (line);
      if (*s == '\0' || *s == '#')
        continue;
      if (strcmp (s, login) == 0)
        {
          fclose (f);
          return 1;
        }
    }
  fclose (f);
  return 0;
}