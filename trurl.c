/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <curl/mprintf.h>

#include <locale.h> /* for setlocale() */

#include "version.h"

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define strdup _strdup
#endif

#if CURL_AT_LEAST_VERSION(7,81,0)
#define SUPPORTS_ZONEID
#endif
#if CURL_AT_LEAST_VERSION(7,80,0)
#define SUPPORTS_URL_STRERROR
#endif
#if CURL_AT_LEAST_VERSION(7,78,0)
#define SUPPORTS_ALLOW_SPACE
#else
#define CURLU_ALLOW_SPACE 0
#endif
#if CURL_AT_LEAST_VERSION(7,88,0)
#define SUPPORTS_PUNYCODE
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

#define REPLACE_NULL_BYTE '.' /* for query:key extractions */

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
#define ERROR_GET   10 /* bad --get syntax */
#define ERROR_ITER  11 /* bad --iterate syntax */

#ifndef SUPPORTS_URL_STRERROR
/* provide a fake local mockup */
static char *curl_url_strerror(CURLUcode error)
{
  static char buffer[128];
  curl_msnprintf(buffer, sizeof(buffer), "URL error %u", (int)error);
  return buffer;
}
#endif

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

#define VERIFY(op, exit_code, ...) \
  do { \
    if(op->verify) { \
      /* make sure to terminate the JSON array */ \
      if(op->jsonout) \
        fputs("\n]\n", stdout); \
      errorf(exit_code, __VA_ARGS__); \
    } else \
      warnf(__VA_ARGS__); \
  } while(0)

static void help(void)
{
  int i;
  fputs(
    "Usage: " PROGNAME " [options] [URL]\n"
    "  -a, --append [component]=[data]  - append data to component\n"
    "      --accept-space               - give in to this URL abuse\n"
    "  -f, --url-file [file/-]          - read URLs from file or stdin\n"
    "  -g, --get [{component}s]         - output component(s)\n"
    "  -h, --help                       - this help\n"
    "      --iterate [component]=[list] - create multiple URL outputs\n"
    "      --json                       - output URL as JSON\n"
    "      --no-guess-scheme            - require scheme in URLs\n"
    "      --query-separator [letter]   - if something else than '&'\n"
    "      --redirect [URL]             - redirect to this\n"
    "  -s, --set [component]=[data]     - set component content\n"
    "      --sort-query                 - alpha-sort the query pairs\n"
    "      --trim [component]=[what]    - trim component\n"
    "      --url [URL]                  - URL to work with\n"
    "  -v, --version                    - show version\n"
    "      --verify                     - return error on (first) bad URL\n"
    " URL COMPONENTS:\n"
    "  ", stdout);
  for(i = 0; i< NUM_COMPONENTS; i++) {
    printf("%s%s", i?", ":"", variables[i].name);
  }
  fputs("\n", stdout);
  exit(0);
}

static void show_version(void)
{
  curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
  fprintf(stdout, "%s version %s libcurl/%s [built-with %s]\n",
          PROGNAME, TRURL_VERSION_TXT, data->version, LIBCURL_VERSION);
  exit(0);
}

struct iterinfo {
  CURLU *uh;
  const char *part;
  size_t plen;
  char *ptr;
  unsigned int varmask; /* sets 1 << [component] */
};

struct option {
  struct curl_slist *url_list;
  struct curl_slist *append_path;
  struct curl_slist *append_query;
  struct curl_slist *set_list;
  struct curl_slist *trim_list;
  struct curl_slist *iter_list;
  const char *redirect;
  const char *qsep;
  const char *format;
  FILE *url;
  bool urlopen;
  bool jsonout;
  bool verify;
  bool accept_space;
  bool sort_query;
  bool no_guess_scheme;
  bool end_of_options;
  unsigned char output;

  /* -- stats -- */
  unsigned int urls;
};

#define MAX_QPAIRS 1000
char *qpairs[MAX_QPAIRS]; /* encoded */
char *qpairsdec[MAX_QPAIRS]; /* decoded */
int nqpairs; /* how many is stored */

static char *strurldecode(const char *url, int inlength, int *outlength)
{
  return curl_easy_unescape(NULL, inlength ? url : "", inlength,
                            outlength);
}

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
    curl_free(urle);
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
    curl_free(urle);
  }
}

static void appendadd(struct option *o,
                      const char *arg)
{
  if(!strncmp("path=", arg, 5))
    pathadd(o, arg + 5);
  else if(!strncmp("query=", arg, 6))
    queryadd(o, arg + 6);
  else
    errorf(ERROR_APPEND, "--append unsupported component: %s", arg);
}

static void setadd(struct option *o,
                   const char *set) /* [component]=[data] */
{
  struct curl_slist *n;
  n = curl_slist_append(o->set_list, set);
  if(!strncmp("url=", set, 4))
    errorf(ERROR_SET, "--set does not support url, use --redirect instead");
  if(n)
    o->set_list = n;
}

static void iteradd(struct option *o,
                    const char *iter) /* [component]=[data] */
{
  struct curl_slist *n;
  n = curl_slist_append(o->iter_list, iter);
  if(n)
    o->iter_list = n;
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

  if(!strcmp("--", flag))
    op->end_of_options = true;
  else if(!strcmp("-v", flag) || !strcmp("--version", flag))
    show_version();
  else if(!strcmp("-h", flag) || !strcmp("--help", flag))
    help();
  else if(checkoptarg("--url", flag, arg)) {
    urladd(op, arg);
    *usedarg = true;
  }
  else if(checkoptarg("-f", flag, arg) ||
          checkoptarg("--url-file", flag, arg)) {
    urlfile(op, arg);
    *usedarg = true;
  }
  else if(checkoptarg("-a", flag, arg) ||
          checkoptarg("--append", flag, arg)) {
    appendadd(op, arg);
    *usedarg = true;
  }
  else if(checkoptarg("-s", flag, arg) ||
          checkoptarg("--set", flag, arg)) {
    setadd(op, arg);
    *usedarg = true;
  }
  else if(checkoptarg("--iterate", flag, arg)) {
    iteradd(op, arg);
    *usedarg = true;
  }
  else if(checkoptarg("--redirect", flag, arg)) {
    if(op->redirect)
      errorf(ERROR_FLAG, "only one --redirect is supported");
    op->redirect = arg;
    *usedarg = true;
  }
  else if(checkoptarg("--query-separator", flag, arg)) {
    if(op->qsep)
      errorf(ERROR_FLAG, "only one --query-separator is supported");
    if(strlen(arg) != 1)
      errorf(ERROR_FLAG, "only single-letter query separators are supported");
    op->qsep = arg;
    *usedarg = true;
  }
  else if(checkoptarg("--trim", flag, arg)) {
    trimadd(op, arg);
    *usedarg = true;
  }
  else if(checkoptarg("-g", flag, arg) ||
          checkoptarg("--get", flag, arg)) {
    if(op->format)
      errorf(ERROR_FLAG, "only one --get is supported");
    if(op->jsonout)
      errorf(ERROR_FLAG, "--get is mututally exclusive with --json");
    op->format = arg;
    *usedarg = true;
  }
  else if(!strcmp("--json", flag)) {
    if(op->format)
      errorf(ERROR_FLAG, "--json is mututally exclusive with --get");
    op->jsonout = true;
  }
  else if(!strcmp("--verify", flag))
    op->verify = true;
  else if(!strcmp("--accept-space", flag)) {
#ifdef SUPPORTS_ALLOW_SPACE
    op->accept_space = true;
#else
    warnf("built with too old libcurl version, --accept-space does not work");
#endif
  }
  else if(!strcmp("--no-guess-scheme", flag))
    op->no_guess_scheme = true;
  else if(!strcmp("--sort-query", flag))
    op->sort_query = true;
  else
    return 1;  /* unrecognized option */
  return 0;
}

static void showqkey(const char *key, size_t klen, bool urldecode,
                     bool showall)
{
  int i;
  bool shown = false;
  char **qp = urldecode ? qpairsdec : qpairs;

  for(i = 0; i< nqpairs; i++) {
    if(!strncmp(key, qp[i], klen) &&
       (qp[i][klen] == '=')) {
      if(shown)
        fputc(' ', stdout);
      fputs(&qp[i][klen + 1], stdout);
      if(!showall)
        break;
      shown = true;
    }
  }
}

/* component to variable pointer */
static const struct var *comp2var(const char *name, size_t vlen)
{
  int i;
  for(i = 0; variables[i].name; i++)
    if((strlen(variables[i].name) == vlen) &&
       !strncmp(name, variables[i].name, vlen))
      return &variables[i];
  return NULL;
}

static void get(struct option *op, CURLU *uh)
{
  FILE *stream = stdout;
  const char *ptr = op->format;
  bool done = false;
  char startbyte = 0;
  char endbyte = 0;

  while(ptr && *ptr && !done) {
    if(!startbyte && (('{' == *ptr) || ('[' == *ptr))) {
      startbyte = *ptr;
      if('{' == *ptr)
        endbyte = '}';
      else
        endbyte = ']';
    }
    if(startbyte == *ptr) {
      if(startbyte == ptr[1]) {
        /* an escaped {-letter */
        fputc(startbyte, stream);
        ptr += 2;
      }
      else {
        /* this is meant as a variable to output */
        char *end;
        char *cl;
        size_t vlen;
        bool urldecode = true;
#ifdef SUPPORTS_PUNYCODE
        bool punycode = false;
#endif
        bool handled = true;
        bool rawport = false;
        end = strchr(ptr, endbyte);
        ptr++; /* pass the { */
        if(!end) {
          /* syntax error */
          fputc(startbyte, stream);
          continue;
        }
        /* {path} {:path} */
        if(*ptr == ':') {
          urldecode = false;
          ptr++;
        }
        vlen = end - ptr;
        /* check for the last colon within here */
        cl = memchr(ptr, ':', vlen);
        if(cl) {
          /* deduct the colon part */
          if(!strncmp(ptr, "query-all:", 10))
            showqkey(&ptr[10], end - cl - 1, urldecode, true);
          else if(!strncmp(ptr, "query:", 6))
            showqkey(&ptr[6], end - cl - 1, urldecode, false);
          else if(!strncmp(ptr, "puny:", 5)) {
#ifndef SUPPORTS_PUNYCODE
            warnf("Built without punycode support");
#else
            punycode = true;
#endif
            ptr = cl + 1;
            vlen = end - ptr;
            handled = false;
          }
          else if(!strncmp(ptr, "raw:", 4)) {
            rawport = true;
            ptr = cl + 1;
            vlen = end - ptr;
            handled = false;
          }
          else
            errorf(ERROR_GET, "Bad --get syntax: %s", ptr);
        }
        else
          handled = false;
        if(!handled) {
          const struct var *v = comp2var(ptr, vlen);
          if(v) {
            char *nurl;
            CURLUcode rc;
            rc = curl_url_get(uh, v->part, &nurl,
                              (rawport?0:CURLU_DEFAULT_PORT)|
                              CURLU_NO_DEFAULT_PORT|
#ifdef SUPPORTS_PUNYCODE
                              (punycode?CURLU_PUNYCODE:0)|
#endif
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
#ifdef SUPPORTS_ZONEID
            case CURLUE_NO_ZONEID:
#endif
              /* silently ignore */
              break;
            default:
              fprintf(stderr, PROGNAME ": %s (%s)\n", curl_url_strerror(rc),
                      v->name);
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
      case '\\':
        fputc('\\', stream);
        break;
      case '{':
        fputc('{', stream);
        break;
      case '[':
        fputc('[', stream);
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

static const struct var *setone(CURLU *uh, const char *setline)
{
  char *ptr = strchr(setline, '=');
  const struct var *v = NULL;
  if(ptr && (ptr > setline)) {
    size_t vlen = ptr - setline;
    bool urlencode = true;
    bool found = false;
    if(ptr[-1] == ':') {
      urlencode = false;
      vlen--;
    }
    v = comp2var(setline, vlen);
    if(v) {
      CURLUcode rc;
      rc = curl_url_set(uh, v->part, ptr[1] ? &ptr[1] : NULL,
                        CURLU_NON_SUPPORT_SCHEME|
                        (urlencode ? CURLU_URLENCODE : 0) );
      if(rc)
        warnf("Error setting %s: %s", v->name, curl_url_strerror(rc));
      found = true;
    }
    if(!found)
      errorf(ERROR_SET, "unknown component: %.*s", (int)vlen, setline);
  }
  else
    errorf(ERROR_SET, "invalid --set syntax: %s", setline);
  return v;
}

static unsigned int set(CURLU *uh,
                        struct option *o)
{
  struct curl_slist *node;
  unsigned int mask = 0;
  for(node =  o->set_list; node; node = node->next) {
    const struct var *v;
    char *set = node->data;
    v = setone(uh, set);
    if(v) {
      if(mask & (1 << v->part))
        errorf(ERROR_SET, "duplicate --set for component %s", v->name);
      mask |= (1 << v->part);
    }
  }
  return mask; /* the set components */
}

static void jsonString(FILE *stream, const char *in, size_t len,
                       bool lowercase)
{
  const unsigned char *i = (unsigned char *)in;
  const char *in_end = &in[len];

  fputc('\"', stream);
  for(; i < (unsigned char *)in_end; i++) {
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
      if(*i < 32)
        fprintf(stream, "u%04x", *i);
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
  bool first = true;
  (void)o;
  printf("%s  {\n", o->urls?",\n":"");
  for(i = 0; variables[i].name; i++) {
    char *nurl;
    CURLUcode rc = curl_url_get(uh, variables[i].part, &nurl,
                                (i?CURLU_DEFAULT_PORT:0)|
                                CURLU_URLDECODE);
    if(!rc) {
      if(!first)
        fputs(",\n", stdout);
      first = false;
      printf("    \"%s\": ", variables[i].name);
      jsonString(stdout, nurl, strlen(nurl), false);
      curl_free(nurl);
    }
    /* if we're printing the port, show if it's explicit or not */
    if(variables[i].part == CURLUPART_PORT) {
      rc = curl_url_get(uh, variables[i].part, &nurl,
                CURLU_URLDECODE);
      fputs(",\n", stdout);
      printf("    \"raw_port\": ");
      if(!rc) {
        jsonString(stdout, nurl, strlen(nurl), false);
        curl_free(nurl);
      }
      else
        jsonString(stdout, "", 0, false);
    }
  }
  first = true;
  if(nqpairs) {
    int j;
    fputs(",\n    \"params\": [\n", stdout);
    for(j = 0 ; j < nqpairs; j++) {
      const char *sep = strchr(qpairsdec[j], '=');
      const char *value = sep ? sep + 1 : "";

      /* don't print out empty/trimmed values */
      if(!qpairsdec[j][0])
        continue;
      if(!first)
        fputs(",\n", stdout);
      first = false;
      fputs("      {\n        \"key\": ", stdout);
      jsonString(stdout, qpairsdec[j],
                 sep ? (size_t)(sep - qpairsdec[j]) : strlen(qpairsdec[j]),
                 false);
      fputs(",\n        \"value\": ", stdout);
      jsonString(stdout, value, strlen(value), false);
      fputs("\n      }", stdout);
    }
    fputs("\n    ]", stdout);
  }
  fputs("\n  }", stdout);
}

/* --trim query="utm_*" */
static void trim(struct option *o)
{
  struct curl_slist *node;
  for(node = o->trim_list; node; node = node->next) {
    char *instr = node->data;
    if(strncmp(instr, "query", 5))
      /* for now we can only trim query components */
      errorf(ERROR_TRIM, "Unsupported trim component: %s", instr);
    char *ptr = strchr(instr, '=');
    if(ptr && (ptr > instr)) {
      /* 'ptr' should be a fixed string or a pattern ending with an
         asterisk */
      size_t inslen;
      bool pattern;
      int i;

      ptr++; /* pass the = */
      inslen = strlen(ptr);
      pattern = ptr[inslen - 1] == '*';
      if(pattern)
        inslen--;

      for(i = 0 ; i < nqpairs; i++) {
        char *q = qpairs[i];
        char *sep = strchr(q, '=');
        size_t qlen;
        if(sep)
          qlen = sep - q;
        else
          qlen = strlen(q);

        if((pattern && (inslen <= qlen) &&
            !strncasecmp(q, ptr, inslen)) ||
           (!pattern && (inslen == qlen) &&
            !strncasecmp(q, ptr, inslen))) {
          /* this qpair should be stripped out */
          free(qpairs[i]);
          free(qpairsdec[i]);
          qpairs[i] = strdup(""); /* marked as deleted */
          qpairsdec[i] = strdup(""); /* marked as deleted */
        }
      }
    }
  }
}

/* memdup the amount and add a trailing zero */
static char *memdupzero(char *source, size_t len)
{
  char *p = malloc(len + 1);
  if(p) {
    memcpy(p, source, len);
    p[len] = 0;
    return p;
  }
  return NULL;
}

/* URL decode the pair and return it in an allocated chunk */
static char *memdupdec(char *source, size_t len)
{
  char *sep = memchr(source, '=', len);
  char *left = NULL;
  char *right = NULL;
  char *str, *dup;
  int leftlen = 0;
  int rightlen = 0;

  left = strurldecode(source, sep ? (size_t)(sep - source) : len,
                      &leftlen);
  if(sep) {
    char *p;
    int plen;
    right = strurldecode(sep + 1, len - (sep - source) - 1, &rightlen);

    /* convert null bytes to periods */
    for(plen = rightlen, p = right; plen; plen--, p++) {
      if(!*p)
        *p = REPLACE_NULL_BYTE;
    }
  }

  str = curl_maprintf("%.*s%s%.*s", leftlen, left,
                      right ? "=":"",
                      rightlen, right?right:"");
  curl_free(right);
  curl_free(left);
  dup = strdup(str);
  curl_free(str);
  return dup;
}


static void freeqpairs(void)
{
  int i;
  for(i = 0; i<nqpairs; i++) {
    free(qpairs[i]);
    qpairs[i] = NULL;
    free(qpairsdec[i]);
    qpairsdec[i] = NULL;
  }
  nqpairs = 0;
}

/* store the pair both encoded and decoded */
static char *addqpair(char *pair, size_t len)
{
  char *p = NULL;
  char *pdec = NULL;
  if(nqpairs < MAX_QPAIRS) {
    p = memdupzero(pair, len);
    pdec = memdupdec(pair, len);
    if(p && pdec) {
      qpairs[nqpairs] = p;
      qpairsdec[nqpairs] = pdec;
      nqpairs++;
    }
  }
  else
    warnf("too many query pairs");
  return p;
}

/* convert the query string into an array of name=data pair */
static void extractqpairs(CURLU *uh, struct option *o)
{
  char *q = NULL;
  memset(qpairs, 0, sizeof(qpairs));
  nqpairs = 0;
  /* extract the query */
  if(!curl_url_get(uh, CURLUPART_QUERY, &q, 0)) {
    char *p = q;
    char *amp;
    while(*p) {
      size_t len;
      amp = strchr(p, o->qsep[0]);
      if(!amp)
        len = strlen(p);
      else
        len = amp - p;
      addqpair(p, len);
      if(amp)
        p = amp + 1;
      else
        break;
    }
  }
  curl_free(q);
}

static void qpair2query(CURLU *uh, struct option *o)
{
  int i;
  int rc;
  char *nq = NULL;
  char *oldnq;
  for(i = 0; i<nqpairs; i++) {
    oldnq = nq;
    nq = curl_maprintf("%s%s%s", nq?nq:"",
                       (nq && *nq && *qpairs[i])? o->qsep: "", qpairs[i]);
    curl_free(oldnq);
  }
  if(nq) {
    rc = curl_url_set(uh, CURLUPART_QUERY, nq, 0);
    if(rc)
      warnf("internal problem");
  }
  curl_free(nq);
}

/* sort case insensitively */
static int cmpfunc(const void *p1, const void *p2)
{
  return strcasecmp(*(const char **)p1,
                    *(const char **)p2);
}

static void sortquery(struct option *o)
{
  if(o->sort_query) {
    /* not these two lists may no longer be the same order after the sort */
    qsort(&qpairs[0], nqpairs, sizeof(char *), cmpfunc);
    qsort(&qpairsdec[0], nqpairs, sizeof(char *), cmpfunc);
  }
}

static CURLUcode seturl(struct option *o, CURLU *uh, const char *url)
{
  return curl_url_set(uh, CURLUPART_URL, url,
                      (o->no_guess_scheme ?
                       0 : CURLU_GUESS_SCHEME)|
                      CURLU_NON_SUPPORT_SCHEME|
                      (o->accept_space ?
                       (CURLU_ALLOW_SPACE|CURLU_URLENCODE) : 0));
}

static void singleurl(struct option *o,
                      const char *url, /* might be NULL */
                      struct iterinfo *iinfo,
                      struct curl_slist *iter)
{
  CURLU *uh = iinfo->uh;
  if(!uh) {
    uh = curl_url();
    if(!uh)
      errorf(ERROR_MEM, "out of memory");
    if(url) {
      CURLUcode rc = seturl(o, uh, url);
      if(rc) {
        curl_url_cleanup(uh);
        VERIFY(o, ERROR_BADURL, "%s [%s]", curl_url_strerror(rc), url);
        return;
      }
      if(o->redirect) {
        rc = seturl(o, uh, o->redirect);
        if(rc) {
          curl_url_cleanup(uh);
          VERIFY(o, ERROR_BADURL, "invalid redirection: %s [%s]",
                 curl_url_strerror(rc), o->redirect);
          return;
        }
      }
    }
  }
  do {
    char iterbuf[1024];
    struct curl_slist *p;
    bool url_is_invalid = false;
    unsigned setmask = 0;

    /* set everything */
    setmask = set(uh, o);

    if(iter) {
      /* "part=item1 item2 item2" */
      const char *part;
      size_t plen;
      const char *w;
      size_t wlen;
      char *sep;
      bool urlencode = true;
      const struct var *v;

      if(!iinfo->ptr) {
        part = iter->data;
        sep = strchr(part, '=');
        if(!sep)
          errorf(ERROR_ITER, "wrong iterate syntax");
        plen = sep - part;
        if(sep[-1] == ':') {
          urlencode = false;
          plen--;
        }
        w = sep + 1;
        /* store for next lap */
        iinfo->part = part;
        iinfo->plen = plen;
        v = comp2var(part, plen);
        if(!v)
          errorf(ERROR_ITER, "bad component for iterate");
        if(iinfo->varmask & (1<<v->part))
          errorf(ERROR_ITER, "duplicate component for iterate: %s", v->name);
        if(setmask & (1 << v->part))
          errorf(ERROR_ITER, "duplicate --iterate and --set for component %s",
                 v->name);
      }
      else {
        part = iinfo->part;
        plen = iinfo->plen;
        v = comp2var(part, plen);
        w = iinfo->ptr;
      }

      sep = strchr(w, ' ');
      if(sep) {
        wlen = sep - w;
        iinfo->ptr = sep + 1; /* next word is here */
      }
      else {
        /* last word */
        wlen = strlen(w);
        iinfo->ptr = NULL;
      }
      curl_msnprintf(iterbuf, sizeof(iterbuf), "%.*s%s=%.*s", (int)plen, part,
                     urlencode ? "" : ":",
                     (int)wlen, w);
      setone(uh, iterbuf);
      if(iter && iter->next) {
        struct iterinfo info;
        memset(&info, 0, sizeof(info));
        info.uh = uh;
        info.varmask = iinfo->varmask | (1 << v->part);
        singleurl(o, url, &info, iter->next);
      }
    }

    /* append path segments */
    for(p = o->append_path; p; p = p->next) {
      char *apath = p->data;
      char *opath;
      char *npath;
      size_t olen;
      /* extract the current path */
      curl_url_get(uh, CURLUPART_PATH, &opath, 0);

      /* does the existing path end with a slash, then don't
         add one in between */
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

    extractqpairs(uh, o);

    /* append query segments */
    for(p = o->append_query; p; p = p->next) {
      addqpair(p->data, strlen(p->data));
    }

    /* trim parts */
    trim(o);

    sortquery(o);

    /* put the query back */
    qpair2query(uh, o);

    /* make sure the URL is still valid */
    if(!url || o->redirect || o->set_list || o->append_path) {
      char *ourl = NULL;
      CURLUcode rc = curl_url_get(uh, CURLUPART_URL, &ourl, 0);
      if(rc) {
        VERIFY(o, ERROR_URL, "not enough input for a URL");
        url_is_invalid = true;
      }
      else {
        rc = seturl(o, uh, ourl);
        if(rc) {
          VERIFY(o, ERROR_BADURL, "%s [%s]", curl_url_strerror(rc),
                 ourl);
          url_is_invalid = true;
        }
        else {
          char *nurl = NULL;
          rc = curl_url_get(uh, CURLUPART_URL, &nurl, 0);
          if(rc || strcmp(ourl, nurl)) {
            VERIFY(o, ERROR_BADURL, "url became invalid");
            url_is_invalid = true;
          }
          if(!rc)
            curl_free(nurl);
        }
        curl_free(ourl);
      }
    }

    if(iter && iter->next)
      ;
    else if(url_is_invalid)
      ;
    else if(o->jsonout)
      json(o, uh);
    else if(o->format) {
      /* custom output format */
      get(o, uh);
    }
    else {
      /* default output is full URL */
      char *nurl = NULL;
      if(!curl_url_get(uh, CURLUPART_URL, &nurl, CURLU_NO_DEFAULT_PORT)) {
        printf("%s\n", nurl);
        curl_free(nurl);
      }
    }

    fflush(stdout);

    freeqpairs();

    o->urls++;

  } while(iinfo->ptr);
  if(!iinfo->uh)
    curl_url_cleanup(uh);
}

int main(int argc, const char **argv)
{
  int exit_status = 0;
  struct option o;
  struct curl_slist *node;
  memset(&o, 0, sizeof(o));
  setlocale(LC_ALL, "");
  curl_global_init(CURL_GLOBAL_ALL);

  for(argc--, argv++; argc > 0; argc--, argv++) {
    bool usedarg = false;
    if(!o.end_of_options && argv[0][0] == '-') {
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
  if(!o.qsep)
    o.qsep = "&";

  if(o.jsonout)
    fputs("[\n", stdout);

  if(o.url) {
    /* this is a file to read URLs from */
    char buffer[4096]; /* arbitrary max */
    bool end_of_file = false;
    while(!end_of_file && fgets(buffer, sizeof(buffer), o.url)) {
      char *eol = strchr(buffer, '\n');
      if(eol && (eol > buffer)) {
        if(eol[-1] == '\r')
          /* CRLF detected */
          eol--;
      }
      else if(eol == buffer) {
        /* empty line */
        continue;
      }
      else if(feof(o.url)) {
        /* end of file */
        eol = strlen(buffer) + buffer;
        end_of_file = true;
      }
      else {
        /* line too long */
        int ch;
        warnf("skipping long line");
        do {
          ch = getc(o.url);
        } while(ch != EOF && ch != '\n');
        if(ch == EOF) {
          if(ferror(o.url))
            warnf("getc: %s", strerror(errno));
          end_of_file = true;
        }
        continue;
      }

      /* trim trailing spaces and tabs */
      while((eol > buffer) &&
            ((eol[-1] == ' ') || eol[-1] == '\t'))
        eol--;

      if(eol > buffer) {
        /* if there is actual content left to deal with */
        struct iterinfo iinfo;
        memset(&iinfo, 0, sizeof(iinfo));
        *eol = 0; /* end of URL */
        singleurl(&o, buffer, &iinfo, o.iter_list);
      }
    }

    if(!end_of_file && ferror(o.url))
      warnf("fgets: %s", strerror(errno));
    if(o.urlopen)
      fclose(o.url);
  }
  else {
    /* not reading URLs from a file */
    node = o.url_list;
    do {
      if(node) {
        const char *url = node->data;
        struct iterinfo iinfo;
        memset(&iinfo, 0, sizeof(iinfo));
        singleurl(&o, url, &iinfo, o.iter_list);
        node = node->next;
      }
      else {
        struct iterinfo iinfo;
        memset(&iinfo, 0, sizeof(iinfo));
        o.verify = true;
        singleurl(&o, NULL, &iinfo, o.iter_list);
      }
    } while(node);
  }
  if(o.jsonout)
    fputs("\n]\n", stdout);
  /* we're done with libcurl, so clean it up */
  curl_slist_free_all(o.url_list);
  curl_slist_free_all(o.set_list);
  curl_slist_free_all(o.iter_list);
  curl_slist_free_all(o.trim_list);
  curl_slist_free_all(o.append_path);
  curl_slist_free_all(o.append_query);
  curl_global_cleanup();
  return exit_status;
}
