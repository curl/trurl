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

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif

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
#define NUM_COMPONENTS 11 /* including "url" */

#define PROGNAME        "trurl"

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

#define ERROR_PREFIX PROGNAME " error: "
#define WARN_PREFIX PROGNAME " note: "

/* error codes */
#define ERROR_FILE   1
#define ERROR_APPEND 2 /* --append mistake */
#define ERROR_ARG    3 /* a command line option misses its argument */
#define ERROR_FLAG   4 /* a command line flag mistake */
#define ERROR_SET    5 /* a --set problem */
#define ERROR_MEM    6 /* out of memory */
#define ERROR_URL    7 /* could not get a URL out of the set components */
#define ERROR_TRIM   8 /* a --trim problem */
#define ERROR_BADURL 9 /* if --verify is set and the URL cannot parse */

static void warnf(char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fputs(WARN_PREFIX, stderr);
  vfprintf(stderr, fmt, ap);
  fputs("\n", stderr);
  va_end(ap);
}

static void errorf(int exit_code, char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fputs(ERROR_PREFIX, stderr);
  vfprintf(stderr, fmt, ap);
  fputs("\n" ERROR_PREFIX "Try " PROGNAME " -h for help\n", stderr);
  va_end(ap);
  exit(exit_code);
}


static void help(void)
{
  int i;
  fprintf(stderr, "Usage: " PROGNAME " [options] [URL]\n"
          "  --accept-space               - give in to this URL abuse\n"
          "  --append [component]=[data]  - append data to component\n"
          "  -f, --url-file [file/-]      - read URLs from file or stdin\n"
          "  -g, --get [{component}s]     - output URL component(s)\n"
          "  -h, --help                   - this help\n"
          " --json                        - output URL info as JSON\n"
          "  --redirect [URL]             - redirect the base URL to this\n"
          "  -s, --set [component]=[data] - set this component\n"
          "  --trim [component]=[what]    - trim a component\n"
          "  --url [base URL]             - URL to start with\n"
          "  -v, --version                - show version\n"
          " --verify                      - return error on (first) bad URL\n"
          " URL COMPONENTS:\n"
          "  "
    );
  for(i=0; i< NUM_COMPONENTS; i++) {
    fprintf(stderr, "%s%s", i?", ":"", variables[i].name);
  }
  fprintf(stderr, "\n");
  exit(1);
}

static void show_version(void)
{
  curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
  fprintf(stdout, "%s version %s libcurl/%s\n", PROGNAME, TRURL_VERSION_TXT,
          data->version);
  exit(0);
}

struct option {
  struct curl_slist *url_list;
  struct curl_slist *append_path;
  struct curl_slist *append_query;
  struct curl_slist *set_list;
  struct curl_slist *trim_list;
  const char *redirect;
  const char *format;
  FILE *url;
  bool urlopen;
  bool jsonout;
  bool verify;
  bool accept_space;
  unsigned char output;

  /* -- stats -- */
  unsigned int urls;
};

static void urladd(struct option *o, const char *url)
{
  struct curl_slist *n;
  n = curl_slist_append(o->url_list, url);
  if(n)
    o->url_list = n;
}


/* read URLs from this file/stdin */
static void urlfile(struct option *o, const char *file)
{
  FILE *f;
  if(o->url)
    errorf(ERROR_FLAG, "only one --url-file is supported");
  if(strcmp("-", file)) {
    f = fopen(file, "rt");
    if(!f)
      errorf(ERROR_FILE, "--url-file %s not found", file);
    o->urlopen = true;
  }
  else
    f = stdin;
  o->url = f;
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
    free(urle);
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
    free(urle);
  }
}

static void appendadd(struct option *o,
                      const char *arg)
{
  if(!strncasecmp("path=", arg, 5))
    pathadd(o, arg + 5);
  else if(!strncasecmp("query=", arg, 6))
    queryadd(o, arg + 6);
  else
    errorf(ERROR_APPEND, "--append unsupported component: %s", arg);
}

static void setadd(struct option *o,
                   const char *set) /* [component]=[data] */
{
  struct curl_slist *n;
  n = curl_slist_append(o->set_list, set);
  if(n)
    o->set_list = n;
}

static void trimadd(struct option *o,
                    const char *trim) /* [component]=[data] */
{
  struct curl_slist *n;
  n = curl_slist_append(o->trim_list, trim);
  if(n)
    o->trim_list = n;
}

static bool checkoptarg(const char *str,
                        const char *given,
                        const char *arg)
{
  if(!strcmp(str, given)) {
    if(!arg)
      errorf(ERROR_ARG, "Missing argument for %s", str);
    return true;
  }
  return false;
}

static int getarg(struct option *op,
                  const char *flag,
                  const char *arg,
                  bool *usedarg)
{
  *usedarg = false;

  if(!strcmp("-v", flag) || !strcmp("--version", flag))
    show_version();
  else if(!strcmp("-h", flag) || !strcmp("--help", flag))
    help();
  else if(checkoptarg("--url", flag, arg)) {
    urladd(op, arg);
    *usedarg = 1;
  }
  else if(checkoptarg("-f", flag, arg) ||
          checkoptarg("--url-file", flag, arg)) {
    urlfile(op, arg);
    *usedarg = 1;
  }
  else if(checkoptarg("--append", flag, arg)) {
    appendadd(op, arg);
    *usedarg = 1;
  }
  else if(checkoptarg("-s", flag, arg) ||
          checkoptarg("--set", flag, arg)) {
    setadd(op, arg);
    *usedarg = 1;
  }
  else if(checkoptarg("--redirect", flag, arg)) {
    if(op->redirect)
      errorf(ERROR_FLAG, "only one --redirect is supported");
    op->redirect = arg;
    *usedarg = 1;
  }
  else if(checkoptarg("--trim", flag, arg)) {
    trimadd(op, arg);
    *usedarg = 1;
  }
  else if(checkoptarg("-g", flag, arg) ||
          checkoptarg("--get", flag, arg)) {
    if(op->format)
      errorf(ERROR_FLAG, "only one --get is supported");
    op->format = arg;
    *usedarg = 1;
  }
  else if(!strcmp("--json", flag))
    op->jsonout = true;
  else if(!strcmp("--verify", flag))
    op->verify = true;
  else if(!strcmp("--accept-space", flag))
    op->accept_space = true;
  else
    return 1;  /* unrecognized option */
  return 0;
}

static void get(struct option *op, CURLU *uh)
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
        bool urldecode = true;
        end = strchr(ptr, '}');
        ptr++; /* pass the { */
        if(!end) {
          /* syntax error */
          continue;
        }
        /* {path} {:path} */
        if(*ptr == ':') {
          urldecode = false;
          ptr++;
        }
        vlen = end - ptr;
        for(i = 0; variables[i].name; i++) {
          if((strlen(variables[i].name) == vlen) &&
             !strncasecmp(ptr, variables[i].name, vlen)) {
            char *nurl;
            CURLUcode rc;
            rc = curl_url_get(uh, variables[i].part, &nurl, CURLU_DEFAULT_PORT|
                              (urldecode?CURLU_URLDECODE:0));
            switch(rc) {
            case CURLUE_OK:
              fprintf(stream, "%s", nurl);
              curl_free(nurl);
            case CURLUE_NO_SCHEME:
            case CURLUE_NO_USER:
            case CURLUE_NO_PASSWORD:
            case CURLUE_NO_OPTIONS:
            case CURLUE_NO_HOST:
            case CURLUE_NO_PORT:
            case CURLUE_NO_QUERY:
            case CURLUE_NO_FRAGMENT:
            case CURLUE_NO_ZONEID:
              /* silently ignore */
              break;
            default:
              fprintf(stderr, PROGNAME ": %s (%s)\n", curl_url_strerror(rc),
                      variables[i].name);
              break;
            }
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
  bool varset[NUM_COMPONENTS];
  memset(varset, 0, sizeof(varset));
  for(node =  o->set_list; node; node=node->next) {
    char *set = node->data;
    int i;
    char *ptr = strchr(set, '=');
    if(ptr && (ptr > set)) {
      size_t vlen = ptr-set;
      bool urlencode = true;
      bool found = false;
      if(ptr[-1] == ':') {
        urlencode = false;
        vlen--;
      }
      for(i=0; variables[i].name; i++) {
        if((strlen(variables[i].name) == vlen) &&
           !strncasecmp(set, variables[i].name, vlen)) {
          if(varset[i])
            errorf(ERROR_SET, "A component can only be set once per URL (%s)",
                   variables[i].name);
          curl_url_set(uh, variables[i].part, ptr+1,
                       CURLU_NON_SUPPORT_SCHEME|
                       (urlencode ? CURLU_URLENCODE : 0) );
          found = true;
          varset[i] = true;
          break;
        }
      }
      if(!found)
        errorf(ERROR_SET, "Set unknown component: %s", set);
    }
    else
      errorf(ERROR_SET, "invalid --set syntax: %s", set);
  }
}

static void jsonString(FILE *stream, const char *in, bool lowercase)
{
  const char *i = in;
  const char *in_end = in + strlen(in);

  fputc('\"', stream);
  for(; i < in_end; i++) {
    switch(*i) {
    case '\\':
      fputs("\\\\", stream);
      break;
    case '\"':
      fputs("\\\"", stream);
      break;
    case '\b':
      fputs("\\b", stream);
      break;
    case '\f':
      fputs("\\f", stream);
      break;
    case '\n':
      fputs("\\n", stream);
      break;
    case '\r':
      fputs("\\r", stream);
      break;
    case '\t':
      fputs("\\t", stream);
      break;
    default:
      if (*i < 32) {
        fprintf(stream, "u%04x", *i);
      }
      else {
        char out = *i;
        if(lowercase && (out >= 'A' && out <= 'Z'))
          /* do not use tolower() since that's locale specific */
          out |= ('a' - 'A');
        fputc(out, stream);
      }
      break;
    }
  }
  fputc('\"', stream);
}

static void json(struct option *o, CURLU *uh)
{
  int i;
  (void)o;
  printf("%s  {\n", o->urls?",\n":"");
  for(i = 0; variables[i].name; i++) {
    char *nurl;
    CURLUcode rc = curl_url_get(uh, variables[i].part, &nurl,
                                (i?CURLU_DEFAULT_PORT:0)|
                                CURLU_URLDECODE);
    if(!rc) {
      if(i)
        fputs(",\n", stdout);
      printf("    \"%s\": ", variables[i].name);
      jsonString(stdout, nurl, false);
    }
  }
  fputs("\n  }", stdout);
}

/* --trim query="utm_*" */
static void trim(CURLU *uh, struct option *o)
{
  struct curl_slist *node;
  for(node = o->trim_list; node; node=node->next) {
    char *instr = node->data;
    if(strncasecmp(instr, "query", 5))
      /* for now we can only trim query components */
      errorf(ERROR_TRIM, "Unsupported trim component: %s", instr);
    char *ptr = strchr(instr, '=');
    if(ptr && (ptr > instr)) {
      /* 'ptr' should be a fixed string or a pattern ending with an
         asterisk */
      size_t inslen;
      bool pattern;
      char *q;
      CURLUcode rc;
      char *newq;

      ptr++; /* pass the = */
      inslen = strlen(ptr);
      pattern = ptr[inslen - 1] == '*';
      if(pattern)
        inslen--;
      rc = curl_url_get(uh, CURLUPART_QUERY, &q, 0);
      if(!rc && q) {
        bool updated;
        newq = q;
        /* q is assumed to look like a=b&c=d&e=f */
        do {
          char *sep = strchr(q, '=');
          if(sep) {
            size_t qlen = sep - q;
            if((pattern && (inslen <= qlen) &&
                !strncasecmp(q, ptr, inslen)) ||
               (!pattern && (inslen == qlen) &&
                !strncasecmp(q, ptr, inslen))) {
              /* this part should be stripped out */
              char *end = strchr(sep, '&');
              if(end) {
                end++;
                /* move the rest of the string to this position */
                memmove(q, end, strlen(end)+1);
              }
              else {
                /* don't leave a trailing ampersand */
                if((q > newq) && (q[-1] == '&'))
                  q--;
                *q = 0;
              }
              updated = true;
            }
            else {
              q = strchr(sep, '&');
              if(!q)
                break;
              q++;
            }
          }
        } while(*q);
        if(updated)
          rc = curl_url_set(uh, CURLUPART_QUERY, newq, 0);
        curl_free(newq);
      }
    }
  }
}

static void singleurl(struct option *o,
                      const char *url) /* might be NULL */
{
    struct curl_slist *p;
    CURLU *uh = curl_url();
    if(!uh)
      errorf(ERROR_MEM, "out of memory");
    if(url) {
      CURLUcode rc =
        curl_url_set(uh, CURLUPART_URL, url,
                     CURLU_GUESS_SCHEME|CURLU_NON_SUPPORT_SCHEME|
                     (o->accept_space ?
                      (CURLU_ALLOW_SPACE|CURLU_URLENCODE) : 0));
      if(rc) {
        if(o->verify)
          errorf(ERROR_BADURL, "%s [%s]", curl_url_strerror(rc), url);
        warnf("%s [%s]", curl_url_strerror(rc), url);
      }
      else {
        if(o->redirect)
          curl_url_set(uh, CURLUPART_URL, o->redirect,
                       CURLU_GUESS_SCHEME|CURLU_NON_SUPPORT_SCHEME);
      }
    }
    /* set everything */
    set(uh, o);

    /* append path segments */
    for(p = o->append_path; p; p=p->next) {
      char *apath = p->data;
      char *opath;
      char *npath;
      size_t olen;
      /* extract the current path */
      curl_url_get(uh, CURLUPART_PATH, &opath, 0);

      /* does the existing path end with a slash, then don't
         add one inbetween */
      olen = strlen(opath);

      /* append the new segment */
      npath = curl_maprintf("%s%s%s", opath,
                            opath[olen-1] == '/' ? "" : "/",
                            apath);
      if(npath) {
        /* set the new path */
        curl_url_set(uh, CURLUPART_PATH, npath, 0);
      }
      curl_free(npath);
      curl_free(opath);
    }

    /* append query segments */
    for(p = o->append_query; p; p=p->next) {
      char *aq = p->data;
      char *oq;
      char *nq;
      /* extract the current query */
      if(!curl_url_get(uh, CURLUPART_QUERY, &oq, 0)) {
        /* append the new segment */
        nq = curl_maprintf("%s&%s", oq, aq);
        if(nq)
          /* set the new query */
          curl_url_set(uh, CURLUPART_QUERY, nq, 0);
        curl_free(nq);
        curl_free(oq);
      }
      else {
        /* no existing query, set the new one */
        curl_url_set(uh, CURLUPART_QUERY, aq, 0);
      }
    }

    /* trim parts */
    trim(uh, o);

    if(o->jsonout)
      json(o, uh);
    else if(o->format) {
      /* custom output format */
      get(o, uh);
    }
    else {
      /* default output is full URL */
      char *nurl = NULL;
      if(!curl_url_get(uh, CURLUPART_URL, &nurl, 0)) {
        printf("%s\n", nurl);
        curl_free(nurl);
      }
      else {
        errorf(ERROR_URL, "not enough input for a URL");
      }
    }
    o->urls++;
    curl_url_cleanup(uh);
}

int main(int argc, const char **argv)
{
  int exit_status = 0;
  struct option o;
  struct curl_slist *node;
  memset(&o, 0, sizeof(o));
  curl_global_init(CURL_GLOBAL_ALL);

  for(argc--, argv++; argc > 0; argc--, argv++) {
    bool usedarg = false;
    if((argv[0][0] == '-' && argv[0][1] != '-') ||
       /* single-dash prefix */
       (argv[0][0] == '-' && argv[0][1] == '-')) {
      /* dash-dash prefixed */
      if(getarg(&o, argv[0], argv[1], &usedarg))
        errorf(ERROR_FLAG, "unknown option: %s", argv[0]);
    }
    else {
      /* this is a URL */
      urladd(&o, argv[0]);
    }
    if(usedarg) {
      /* skip the parsed argument */
      argc--;
      argv++;
    }
  }

  if(o.jsonout)
    fputs("[\n", stdout);

  if(o.url) {
    /* this is a file to read URLs from */
    char buffer[4096]; /* arbitrary max */
    while(fgets(buffer, sizeof(buffer), o.url)) {
      char *eol = strchr(buffer, '\n');
      if(eol && (eol > buffer)) {
        if(eol[-1] == '\r')
          /* CRLF detected */
          eol--;
        *eol = 0; /* end of URL */
        singleurl(&o, buffer);
      }
      else {
        /* no newline or no content, skip */
      }
    }
    if(o.urlopen)
      fclose(o.url);
  }
  else {
    /* not reading URLs from a file */
    node = o.url_list;
    do {
      if(node) {
        const char *url = node->data;
        singleurl(&o, url);
        node = node->next;
      }
      else
        singleurl(&o, NULL);
    } while(node);
  }
  if(o.jsonout)
    fputs("\n]\n", stdout);
  /* we're done with libcurl, so clean it up */
  curl_slist_free_all(o.url_list);
  curl_slist_free_all(o.set_list);
  curl_slist_free_all(o.trim_list);
  curl_slist_free_all(o.append_path);
  curl_slist_free_all(o.append_query);
  curl_global_cleanup();
  return exit_status;
}
