/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <curl/mprintf.h>

#include "version.h"

#define OUTPUT_URL      0  /* default */
#define OUTPUT_SCHEME   1
#define OUTPUT_USER     2
#define OUTPUT_PASSWORD 3
#define OUTPUT_OPTIONS  4
#define OUTPUT_HOST     5
#define OUTPUT_PORT     6
#define OUTPUT_PATH     7
#define OUTPUT_QUERY    8
#define OUTPUT_FRAGMENT 9
#define OUTPUT_ZONEID   10
#define PROGNAME        "urler"

static void help(const char *msg)
{
  if(msg != NULL)
    fprintf(stderr, "%s:\n\n", msg);
  fprintf(stderr, "Usage: [options] [URL]\n"
          "  -h,--help             - this help\n"
          "  -v,--version          - show version\n"
          " INPUT\n"
          "  --append-path [segtment]  - add a piece to the path\n"
          "  --append-query [segtment] - add a piece to the query\n"
          "  --redirect [URL]          - redirect the base URL to this\n"
          "  --set-fragment [fragment] - set this fragment\n"
          "  --set-host [host]         - set this host name\n"
          "  --set-options [options]   - set these options\n"
          "  --set-password [secret]   - set this password\n"
          "  --set-path [path]         - set this path\n"
          "  --set-port [port]         - set this port number\n"
          "  --set-query [query]       - set this query\n"
          "  --set-scheme [scheme]     - set this scheme\n"
          "  --set-user [name]         - set this user\n"
          "  --set-zoneid [zoneid]     - set this zone id\n"
          "  --url [base URL]          - URL to start with\n"
          " OUTPUT\n"
          "  --get-fragment  - output only the fragment part\n"
          "  --get-host      - output only the host part\n"
          "  --get-options   - output only the options part\n"
          "  --get-password  - output only the password part\n"
          "  --get-path      - output only the path part\n"
          "  --get-port      - output only the port part\n"
          "  --get-query     - output only the query part\n"
          "  --get-scheme    - output only the scheme part\n"
          "  --get-user      - output only the user part\n"
          "  --get-zoneid    - output only the zoneid part\n"
          "  --get [format]  - output custom format\n"
          " MODIFIERS\n"
          "  --urldecode     - URL decode the output\n"
    );
  exit(1);
}

static void show_version(void)
{
  fputs(PROGNAME " version " URLER_VERSION_TXT "\n", stdout);
  exit(0);
}

struct option {
  struct curl_slist *url_list;
  struct curl_slist *append_path;
  struct curl_slist *append_query;
  const char *host;
  const char *scheme;
  const char *port;
  const char *user;
  const char *password;
  const char *options;
  const char *path;
  const char *query;
  const char *fragment;
  const char *zoneid;
  const char *redirect;
  const char *format;

  unsigned int urldecode:1;
  unsigned char output;
};

static void urladd(struct option *o, const char *url)
{
  struct curl_slist *n;
  n = curl_slist_append(o->url_list, url);
  if(n)
    o->url_list = n;
}

static void pathadd(struct option *o, const char *path)
{
  struct curl_slist *n;
  char *urle = curl_easy_escape(NULL, path, 0);
  if(urle) {
    n = curl_slist_append(o->append_path, urle);
    if(n) {
      o->append_path = n;
    }
  }
}

static void queryadd(struct option *o, const char *query)
{
  struct curl_slist *n;
  char *p = strchr(query, '=');
  char *urle;
  if(p) {
    /* URL encode the left and the right side of the '=' separately */
    char *f1 = curl_easy_escape(NULL, query, p - query);
    char *f2 = curl_easy_escape(NULL, p + 1, 0);
    urle = curl_maprintf("%s=%s", f1, f2);
    curl_free(f1);
    curl_free(f2);
  }
  else
    urle = curl_easy_escape(NULL, query, 0);
  if(urle) {
    n = curl_slist_append(o->append_query, urle);
    if(n) {
      o->append_query = n;
    }
  }
}


static int getlongarg(struct option *op,
                      const char *flag,
                      const char *arg,
                      int *usedarg)
{
  *usedarg = 0;
  if(!strcmp("--help", flag))
    help(NULL);
  if(!strcmp("--version", flag))
    show_version();
  if(!strcmp("--url", flag)) {
    urladd(op, arg);
    *usedarg = 1;
  }
  else if(!strcmp("--append-path", flag)) {
    pathadd(op, arg);
    *usedarg = 1;
  }
  else if(!strcmp("--append-query", flag)) {
    queryadd(op, arg);
    *usedarg = 1;
  }
  else if(!strcmp("--set-host", flag)) {
    op->host = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-scheme", flag)) {
    op->scheme = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-port", flag)) {
    op->port = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-user", flag)) {
    op->user = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-password", flag)) {
    op->password = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-options", flag)) {
    op->options = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-path", flag)) {
    op->path = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-query", flag)) {
    op->query = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-fragment", flag)) {
    op->fragment = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--set-zoneid", flag)) {
    op->zoneid = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--redirect", flag)) {
    op->redirect = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--get-scheme", flag))
    op->output = OUTPUT_SCHEME;
  else if(!strcmp("--get-user", flag))
    op->output = OUTPUT_USER;
  else if(!strcmp("--get-password", flag))
    op->output = OUTPUT_PASSWORD;
  else if(!strcmp("--get-options", flag))
    op->output = OUTPUT_OPTIONS;
  else if(!strcmp("--get-host", flag))
    op->output = OUTPUT_HOST;
  else if(!strcmp("--get-port", flag))
    op->output = OUTPUT_PORT;
  else if(!strcmp("--get-path", flag))
    op->output = OUTPUT_PATH;
  else if(!strcmp("--get-query", flag))
    op->output = OUTPUT_QUERY;
  else if(!strcmp("--get-fragment", flag))
    op->output = OUTPUT_FRAGMENT;
  else if(!strcmp("--get-zoneid", flag))
    op->output = OUTPUT_ZONEID;
  else if(!strcmp("--get", flag)) {
    op->format = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--urldecode", flag))
    op->urldecode = 1;
  else
    return 1;  /* unrecognized option */
  return 0;
}

static int getshortarg(struct option *op,
                       const char *flag)
{
  (void)op;
  if(!strcmp("-v", flag))
    show_version();
  else if(!strcmp("-h", flag))
    help(NULL);
  else
    return 1;  /* unrecognized option */
  return 0;
}

struct var {
  const char *name;
  CURLUPart part;
};

static const struct var variables[] = {
  {"url",      CURLUPART_URL},
  {"scheme",   CURLUPART_SCHEME},
  {"user",     CURLUPART_USER},
  {"password", CURLUPART_PASSWORD},
  {"options",  CURLUPART_OPTIONS},
  {"host",     CURLUPART_HOST},
  {"port",     CURLUPART_PORT},
  {"path",     CURLUPART_PATH},
  {"query",    CURLUPART_QUERY},
  {"fragment", CURLUPART_FRAGMENT},
  {"zoneid",   CURLUPART_ZONEID},
  {NULL, 0}
};

static void format(struct option *op, CURLU *uh)
{
  FILE *stream = stdout;
  const char *ptr = op->format;
  bool done = false;

  while(ptr && *ptr && !done) {
    if('{' == *ptr) {
      if('{' == ptr[1]) {
        /* an escaped {-letter */
        fputc('{', stream);
        ptr += 2;
      }
      else {
        /* this is meant as a variable to output */
        char *end;
        size_t vlen;
        int i;
        end = strchr(ptr, '}');
        ptr++; /* pass the { */
        if(!end) {
          /* syntax error */
          continue;
        }
        vlen = end - ptr;
        for(i = 0; variables[i].name; i++) {
          if((strlen(variables[i].name) == vlen) &&
             !strncasecmp(ptr, variables[i].name, vlen)) {
            char *nurl;
            if(!curl_url_get(uh, variables[i].part, &nurl, CURLU_DEFAULT_PORT|
                             (op->urldecode?CURLU_URLDECODE:0))) {
              fprintf(stream, "%s", nurl);
              curl_free(nurl);
            }
            break;
          }
        }
        ptr = end + 1; /* pass the end */
      }
    }
    else if('\\' == *ptr && ptr[1]) {
      switch(ptr[1]) {
      case 'r':
        fputc('\r', stream);
        break;
      case 'n':
        fputc('\n', stream);
        break;
      case 't':
        fputc('\t', stream);
        break;
      default:
        /* unknown, just output this */
        fputc(*ptr, stream);
        fputc(ptr[1], stream);
        break;
      }
      ptr += 2;
    }
    else {
      fputc(*ptr, stream);
      ptr++;
    }
  }
  fputc('\n', stream);
}

int main(int argc, const char **argv)
{
  int exit_status = 0;
  char *nurl = NULL;
  struct option o;
  CURLU *uh;
  struct curl_slist *node;
  memset(&o, 0, sizeof(o));
  curl_global_init(CURL_GLOBAL_ALL);

  for(argc--, argv++; argc > 0; argc--, argv++) {
    if(argv[0][0] == '-' && argv[0][1] != '-') {
      /* single-dash prefix */
      if(getshortarg(&o, argv[0]))
        help("unknown option");
    }
    else if(argv[0][0] == '-' && argv[0][1] == '-') {
      /* dash-dash prefixed */
      int usedarg = 0;
      if(getlongarg(&o, argv[0], argv[1], &usedarg))
        help("unknown option");

      if(usedarg) {
        /* skip the parsed argument */
        argc--;
        argv++;
      }
    }
    else {
      /* this is a URL */
      urladd(&o, argv[0]);
    }
  }

  node = o.url_list;
  do {
    const char *url = NULL;
    struct curl_slist *p;
    uh = curl_url();
    if(!uh)
      help("out of memory");
    if(node) {
      url = node->data;
      curl_url_set(uh, CURLUPART_URL, url,
                   CURLU_GUESS_SCHEME|CURLU_NON_SUPPORT_SCHEME);
      if(o.redirect)
        curl_url_set(uh, CURLUPART_URL, o.redirect,
                     CURLU_GUESS_SCHEME|CURLU_NON_SUPPORT_SCHEME);
    }
    if(o.host)
      curl_url_set(uh, CURLUPART_HOST, o.host, 0);
    if(o.scheme)
      curl_url_set(uh, CURLUPART_SCHEME, o.scheme, CURLU_NON_SUPPORT_SCHEME);
    if(o.port)
      curl_url_set(uh, CURLUPART_PORT, o.port, 0);
    if(o.user)
      curl_url_set(uh, CURLUPART_USER, o.user, 0);
    if(o.password)
      curl_url_set(uh, CURLUPART_PASSWORD, o.password, 0);
    if(o.options)
      curl_url_set(uh, CURLUPART_OPTIONS, o.options, 0);
    if(o.path)
      curl_url_set(uh, CURLUPART_PATH, o.path, 0);
    if(o.query)
      curl_url_set(uh, CURLUPART_QUERY, o.query, 0);
    if(o.fragment)
      curl_url_set(uh, CURLUPART_FRAGMENT, o.fragment, 0);
    if(o.zoneid)
      curl_url_set(uh, CURLUPART_ZONEID, o.zoneid, 0);

    /* append path segments */
    for(p = o.append_path; p; p=p->next) {
      char *apath = p->data;
      char *opath;
      char *npath;
      /* extract the current path */
      curl_url_get(uh, CURLUPART_PATH, &opath, 0);

      /* append the new segment */
      npath = curl_maprintf("%s/%s", opath, apath);
      if(npath) {
        /* set the new path */
        curl_url_set(uh, CURLUPART_PATH, npath, 0);
      }
      curl_free(npath);
      curl_free(opath);
    }

    /* append query segments */
    for(p = o.append_query; p; p=p->next) {
      char *aq = p->data;
      char *oq;
      char *nq;
      /* extract the current query */
      curl_url_get(uh, CURLUPART_QUERY, &oq, 0);

      /* append the new segment */
      nq = curl_maprintf("%s&%s", oq, aq);
      if(nq) {
        /* set the new query */
        curl_url_set(uh, CURLUPART_QUERY, nq, 0);
      }
      curl_free(nq);
      curl_free(oq);
    }

    if(o.format) {
      /* custom output format */
      format(&o, uh);
    }
    else if(o.output) {
      CURLUPart cpart = CURLUPART_HOST;
      const char *name = NULL;

      /* only extract the part we want to show */
      switch(o.output) {
      case OUTPUT_SCHEME:
        cpart = CURLUPART_SCHEME;
        name = "scheme";
        break;
      case OUTPUT_USER:
        cpart = CURLUPART_USER;
        name = "user";
        break;
      case OUTPUT_PASSWORD:
        cpart = CURLUPART_PASSWORD;
        name = "password";
        break;
      case OUTPUT_OPTIONS:
        cpart = CURLUPART_OPTIONS;
        name = "options";
        break;
      case OUTPUT_HOST:
        cpart = CURLUPART_HOST;
        name = "host";
        break;
      case OUTPUT_PORT:
        cpart = CURLUPART_PORT;
        name = "port";
        break;
      case OUTPUT_PATH:
        cpart = CURLUPART_PATH;
        name = "path";
        break;
      case OUTPUT_QUERY:
        cpart = CURLUPART_QUERY;
        name = "query";
        break;
      case OUTPUT_FRAGMENT:
        cpart = CURLUPART_FRAGMENT;
        name = "fragment";
        break;
      case OUTPUT_ZONEID:
        cpart = CURLUPART_ZONEID;
        name = "zoneid";
        break;
      default:
        fprintf(stderr, "internal error, file an issue!\n");
        break;
      }
      if(!curl_url_get(uh, cpart, &nurl, CURLU_DEFAULT_PORT|
                       (o.urldecode?CURLU_URLDECODE:0))) {
        printf("%s\n", nurl);
        curl_free(nurl);
      }
      else {
        if(url) {
          /* if a URL was given, this just means that the URL did not have
             this component */
        }
        else {
          fprintf(stderr, "not enough input to show %s (%s -h for help)\n",
                  name, PROGNAME);
          exit_status = 1;
        }
      }
    }
    else {
      /* default output is full URL */
      if(!curl_url_get(uh, CURLUPART_URL, &nurl,
                       o.urldecode?CURLU_URLDECODE:0)) {
        printf("%s\n", nurl);
        curl_free(nurl);
      }
      else {
        fprintf(stderr, "not enough input for a URL (%s -h for help)\n",
                PROGNAME);
        exit_status = 1;
      }
    }
    curl_url_cleanup(uh);
    if(node)
      node = node->next;
  } while(node);
  /* we're done with libcurl, so clean it up */
  curl_slist_free_all(o.url_list);
  curl_global_cleanup();
  return exit_status;
}
