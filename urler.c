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
          "  --set [component]=[data]  - set this component\n"
          "  --url [base URL]          - URL to start with\n"
          " OUTPUT\n"
          "  --get [format]            - output URL components\n"
          " MODIFIERS\n"
          "  --urldecode               - URL decode the output\n"
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
  struct curl_slist *set_list;
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


static void setadd(struct option *o,
              const char *set) /* [component]=[data] */
{
  struct curl_slist *n;
  n = curl_slist_append(o->set_list, set);
  if(n)
    o->set_list = n;
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
  else if(!strcmp("--set", flag)) {
    setadd(op, arg);
    *usedarg = 1;
  }
  else if(!strcmp("--redirect", flag)) {
    op->redirect = arg;
    *usedarg = 1;
  }
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

static void set(CURLU *uh,
                struct option *o)
{
  struct curl_slist *node;
  for(node =  o->set_list; node; node=node->next) {
    char *set = node->data;
    int i;
    char *ptr = strchr(set, '=');
    if(ptr) {
      size_t vlen = ptr-set;
      bool found = false;
      for(i=0; variables[i].name; i++) {
        if((strlen(variables[i].name) == vlen) &&
           !strncasecmp(set, variables[i].name, vlen)) {
          curl_url_set(uh, variables[i].part, ptr+1, CURLU_NON_SUPPORT_SCHEME);
          found = true;
          break;
        }
      }
      if(!found)
        help("Set unknown component");
    }
  }
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
  while(node) {
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
    /* set everything */
    set(uh, &o);

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
    node = node->next;
  };
  /* we're done with libcurl, so clean it up */
  curl_slist_free_all(o.url_list);
  curl_global_cleanup();
  return exit_status;
}
