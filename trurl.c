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
#include <curl/curl.h>
#include <curl/mprintf.h>
#include <stdint.h>

#if defined(_MSC_VER) && (_MSC_VER < 1800)
typedef enum {
  bool_false = 0,
  bool_true  = 1
} bool;
#define false bool_false
#define true  bool_true
#else
#include <stdbool.h>
#endif

#include <locale.h> /* for setlocale() */

#include "version.h"

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define strdup _strdup
#endif

#if CURL_AT_LEAST_VERSION(7,77,0)
#define SUPPORTS_NORM_IPV4
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
#if CURL_AT_LEAST_VERSION(8,3,0)
#define SUPPORTS_PUNY2IDN
#endif
#if CURL_AT_LEAST_VERSION(7,30,0)
#define SUPPORTS_IMAP_OPTIONS
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

#define NUM_COMPONENTS 10 /* excluding "url" */

#define PROGNAME        "trurl"

#define REPLACE_NULL_BYTE '.' /* for query:key extractions */

enum {
  VARMODIFIER_URLENCODED = 1 << 1,
  VARMODIFIER_DEFAULT    = 1 << 2,
  VARMODIFIER_PUNY       = 1 << 3,
  VARMODIFIER_PUNY2IDN   = 1 << 4,
};

struct var {
  const char *name;
  CURLUPart part;
};

struct string {
  char *str;
  size_t len;
};

static const struct var variables[] = {
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
#define ERROR_REPL  12 /* a --replace problem */

#ifndef SUPPORTS_URL_STRERROR
/* provide a fake local mockup */
static char *curl_url_strerror(CURLUcode error)
{
  static char buffer[128];
  curl_msnprintf(buffer, sizeof(buffer), "URL error %u", (int)error);
  return buffer;
}
#endif

static void message_low(const char *prefix, const char *suffix,
                        const char *fmt, va_list ap)
{
  fputs(prefix, stderr);
  vfprintf(stderr, fmt, ap);
  fputs(suffix, stderr);
}

static void warnf_low(const char *fmt, va_list ap)
{
  message_low(WARN_PREFIX, "\n", fmt, ap);
}

static void warnf(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  warnf_low(fmt, ap);
  va_end(ap);
}

static void help(void)
{
  int i;
  fputs(
    "Usage: " PROGNAME " [options] [URL]\n"
    "  -a, --append [component]=[data]  - append data to component\n"
    "      --accept-space               - give in to this URL abuse\n"
    "      --curl                       - only schemes supported by libcurl\n"
    "      --default-port               - add known default ports\n"
    "  -f, --url-file [file/-]          - read URLs from file or stdin\n"
    "  -g, --get [{component}s]         - output component(s)\n"
    "  -h, --help                       - this help\n"
    "      --iterate [component]=[list] - create multiple URL outputs\n"
    "      --json                       - output URL as JSON\n"
    "      --keep-port                  - keep known default ports\n"
    "      --no-guess-scheme            - require scheme in URLs\n"
    "      --punycode                   - encode hostnames in punycode\n"
    "      --as-idn                     - encode hostnames in idn\n"
    "      --query-separator [letter]   - if something else than '&'\n"
    "      --redirect [URL]             - redirect to this\n"
    "      --replace [data]             - replaces a query [data]\n"
    "      --force-replace [data]       - appends a new query if not found\n"
    "  -s, --set [component]=[data]     - set component content\n"
    "      --sort-query                 - alpha-sort the query pairs\n"
    "      --trim [component]=[what]    - trim component\n"
    "      --url [URL]                  - URL to work with\n"
    "      --urlencode                  - URL encode components by default\n"
    "  -v, --version                    - show version\n"
    "      --verify                     - return error on (first) bad URL\n"
    "      --quiet                      - Suppress (some) notes and comments\n"
    " URL COMPONENTS:\n"
    "  ", stdout);
  fputs("url, ", stdout);
  for(i = 0; i< NUM_COMPONENTS ; i++) {
    printf("%s%s", i?", ":"", variables[i].name);
  }
  fputs("\n", stdout);
  exit(0);
}

static void show_version(void)
{
  curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
  /* puny code isn't guaranteed based on the version, so it must be polled
   * from libcurl */
#if defined(SUPPORTS_PUNYCODE) || defined(SUPPORTS_PUNY2IDN)
  bool supports_puny = (data->features & CURL_VERSION_IDN) != 0;
#endif
#if defined(SUPPORTS_IMAP_OPTIONS)
  bool supports_imap = false;
  const char *const *protocol_name = data->protocols;
  while(*protocol_name && !supports_imap) {
    supports_imap = !strncmp(*protocol_name, "imap", 3);
    protocol_name++;
  }
#endif

  fprintf(stdout, "%s version %s libcurl/%s [built-with %s]\n",
          PROGNAME, TRURL_VERSION_TXT, data->version, LIBCURL_VERSION);
  fprintf(stdout, "features:");
#ifdef SUPPORTS_PUNYCODE
  if(supports_puny)
    fprintf(stdout, " punycode");
#endif
#ifdef SUPPORTS_ALLOW_SPACE
  fprintf(stdout, " white-space");
#endif
#ifdef SUPPORTS_ZONEID
  fprintf(stdout, " zone-id");
#endif
#ifdef SUPPORTS_URL_STRERROR
  fprintf(stdout, " url-strerror");
#endif
#ifdef SUPPORTS_NORM_IPV4
  fprintf(stdout, " normalize-ipv4");
#endif
#ifdef SUPPORTS_IMAP_OPTIONS
  if(supports_imap)
    fprintf(stdout, " imap-options");
#endif
#ifdef SUPPORTS_PUNY2IDN
  if(supports_puny)
    fprintf(stdout, " punycode2idn");
#endif

  fprintf(stdout, "\n");
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
  struct curl_slist *replace_list;
  const char *redirect;
  const char *qsep;
  const char *format;
  FILE *url;
  bool urlopen;
  bool jsonout;
  bool verify;
  bool accept_space;
  bool curl;
  bool default_port;
  bool keep_port;
  bool punycode;
  bool puny2idn;
  bool sort_query;
  bool no_guess_scheme;
  bool urlencode;
  bool end_of_options;
  bool quiet_warnings;
  bool force_replace;

  /* -- stats -- */
  unsigned int urls;
};

static void trurl_warnf(struct option *o, const char *fmt, ...)
{
  if(!o->quiet_warnings) {
    va_list ap;
    va_start(ap, fmt);
    fputs(WARN_PREFIX, stderr);
    vfprintf(stderr, fmt, ap);
    fputs("\n", stderr);
    va_end(ap);
  }
}

#define MAX_QPAIRS 1000
struct string qpairs[MAX_QPAIRS]; /* encoded */
struct string qpairsdec[MAX_QPAIRS]; /* decoded */
int nqpairs; /* how many is stored */
bool query_is_modified = false;

static void trurl_cleanup_options(struct option *o)
{
  if(!o)
    return;
  curl_slist_free_all(o->url_list);
  curl_slist_free_all(o->set_list);
  curl_slist_free_all(o->iter_list);
  curl_slist_free_all(o->append_query);
  curl_slist_free_all(o->trim_list);
  curl_slist_free_all(o->replace_list);
  curl_slist_free_all(o->append_path);
}

static void errorf_low(const char *fmt, va_list ap)
{
  message_low(ERROR_PREFIX, "\n"
              ERROR_PREFIX "Try " PROGNAME " -h for help\n", fmt, ap);
}

static void errorf(struct option *o, int exit_code, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorf_low(fmt, ap);
  va_end(ap);
  trurl_cleanup_options(o);
  curl_global_cleanup();
  exit(exit_code);
}

static void verify(struct option *o, int exit_code, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  if(!o->verify) {
    warnf_low(fmt, ap);
    va_end(ap);
  }
  else {
    /* make sure to terminate the JSON array */
    if(o->jsonout)
      printf("%s]\n", o->urls ? "\n" : "");
    errorf_low(fmt, ap);
    va_end(ap);
    trurl_cleanup_options(o);
    curl_global_cleanup();
    exit(exit_code);
  }
}

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
    errorf(o, ERROR_FLAG, "only one --url-file is supported");
  if(strcmp("-", file)) {
    f = fopen(file, "rt");
    if(!f)
      errorf(o, ERROR_FILE, "--url-file %s not found", file);
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
    char *f1 = curl_easy_escape(NULL, query, (int)(p - query));
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
    errorf(o, ERROR_APPEND, "--append unsupported component: %s", arg);
}

static void setadd(struct option *o,
                   const char *set) /* [component]=[data] */
{
  struct curl_slist *n;
  n = curl_slist_append(o->set_list, set);
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

static void replaceadd(struct option *o,
                       const char *replace_list) /* [component]=[data] */
{
  struct curl_slist *n = NULL;
  if(replace_list)
    n = curl_slist_append(o->replace_list, replace_list);
  else
    errorf(o, ERROR_REPL, "No data passed to replace component");

  if(n)
    o->replace_list = n;
}
static bool checkoptarg(struct option *o, const char *str,
                        const char *given,
                        const char *arg)
{
  if(!strcmp(str, given)) {
    if(!arg)
      errorf(o, ERROR_ARG, "Missing argument for %s", str);
    return true;
  }
  return false;
}

static int getarg(struct option *o,
                  const char *flag,
                  const char *arg,
                  bool *usedarg)
{
  *usedarg = false;

  if(!strcmp("--", flag))
    o->end_of_options = true;
  else if(!strcmp("-v", flag) || !strcmp("--version", flag))
    show_version();
  else if(!strcmp("-h", flag) || !strcmp("--help", flag))
    help();
  else if(checkoptarg(o, "--url", flag, arg)) {
    urladd(o, arg);
    *usedarg = true;
  }
  else if(checkoptarg(o, "-f", flag, arg) ||
          checkoptarg(o, "--url-file", flag, arg)) {
    urlfile(o, arg);
    *usedarg = true;
  }
  else if(checkoptarg(o, "-a", flag, arg) ||
          checkoptarg(o, "--append", flag, arg)) {
    appendadd(o, arg);
    *usedarg = true;
  }
  else if(checkoptarg(o, "-s", flag, arg) ||
          checkoptarg(o, "--set", flag, arg)) {
    setadd(o, arg);
    *usedarg = true;
  }
  else if(checkoptarg(o, "--iterate", flag, arg)) {
    iteradd(o, arg);
    *usedarg = true;
  }
  else if(checkoptarg(o, "--redirect", flag, arg)) {
    if(o->redirect)
      errorf(o, ERROR_FLAG, "only one --redirect is supported");
    o->redirect = arg;
    *usedarg = true;
  }
  else if(checkoptarg(o, "--query-separator", flag, arg)) {
    if(o->qsep)
      errorf(o, ERROR_FLAG, "only one --query-separator is supported");
    if(strlen(arg) != 1)
      errorf(o, ERROR_FLAG,
                   "only single-letter query separators are supported");
    o->qsep = arg;
    *usedarg = true;
  }
  else if(checkoptarg(o, "--trim", flag, arg)) {
    trimadd(o, arg);
    *usedarg = true;
  }
  else if(checkoptarg(o, "-g", flag, arg) ||
          checkoptarg(o, "--get", flag, arg)) {
    if(o->format)
      errorf(o, ERROR_FLAG, "only one --get is supported");
    if(o->jsonout)
      errorf(o, ERROR_FLAG,
                   "--get is mututally exclusive with --json");
    o->format = arg;
    *usedarg = true;
  }
  else if(!strcmp("--json", flag)) {
    if(o->format)
      errorf(o, ERROR_FLAG, "--json is mututally exclusive with --get");
    o->jsonout = true;
  }
  else if(!strcmp("--verify", flag))
    o->verify = true;
  else if(!strcmp("--accept-space", flag)) {
#ifdef SUPPORTS_ALLOW_SPACE
    o->accept_space = true;
#else
    trurl_warnf(o,
        "built with too old libcurl version, --accept-space does not work");
#endif
  }
  else if(!strcmp("--curl", flag))
    o->curl = true;
  else if(!strcmp("--default-port", flag))
    o->default_port = true;
  else if(!strcmp("--keep-port", flag))
    o->keep_port = true;
  else if(!strcmp("--punycode", flag)) {
    if(o->puny2idn)
      errorf(o, ERROR_FLAG, "--punycode is mutually exclusive with --as-idn");
    o->punycode = true;
  }
  else if(!strcmp("--as-idn", flag)) {
    if(o->punycode)
      errorf(o, ERROR_FLAG, "--as-idn is mutually exclusive with --punycode");
    o->puny2idn = true;
  }
  else if(!strcmp("--no-guess-scheme", flag))
    o->no_guess_scheme = true;
  else if(!strcmp("--sort-query", flag))
    o->sort_query = true;
  else if(!strcmp("--urlencode", flag))
    o->urlencode = true;
  else if(!strcmp("--quiet", flag))
    o->quiet_warnings = true;
  else if(!strcmp("--replace", flag)) {
    replaceadd(o, arg);
    *usedarg = true;
  }
  else if(!strcmp("--force-replace", flag)) {
    replaceadd(o, arg);
    o->force_replace = true;
    *usedarg = true;
  }
  else
    return 1;  /* unrecognized option */
  return 0;
}

static void showqkey(FILE *stream, const char *key, size_t klen,
                     bool urldecode, bool showall)
{
  int i;
  bool shown = false;
  struct string *qp = urldecode ? qpairsdec : qpairs;

  for(i = 0; i< nqpairs; i++) {
    if(!strncmp(key, qp[i].str, klen) && (qp[i].str[klen] == '=')) {
      if(shown)
        fputc(' ', stream);
      fprintf(stream, "%.*s", (int) (qp[i].len - klen - 1),
              &qp[i].str[klen + 1]);
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

static CURLUcode geturlpart(struct option *o, int modifiers, CURLU *uh,
                            CURLUPart part, char **out)
{
  CURLUcode rc = curl_url_get(uh, part, out,
                      (((modifiers & VARMODIFIER_DEFAULT) ||
                        o->default_port) ?
                       CURLU_DEFAULT_PORT :
                       ((part != CURLUPART_URL || o->keep_port) ?
                        0 : CURLU_NO_DEFAULT_PORT))|
#ifdef SUPPORTS_PUNYCODE
                      (((modifiers & VARMODIFIER_PUNY) || o->punycode) ?
                       CURLU_PUNYCODE : 0)|
#endif
#ifdef SUPPORTS_PUNY2IDN
                       (((modifiers & VARMODIFIER_PUNY2IDN) || o->puny2idn) ?
                        CURLU_PUNY2IDN : 0) |
#endif
                      (o->curl ? 0 : CURLU_NON_SUPPORT_SCHEME)|
                      (((modifiers & VARMODIFIER_URLENCODED) ||
                        o->urlencode) ?
                       0 :CURLU_URLDECODE));

#ifdef SUPPORTS_PUNY2IDN
    /* retry get w/ out puny2idn to handle invalid punycode conversions */
    if(rc == CURLUE_BAD_HOSTNAME &&
            (o->puny2idn || (modifiers & VARMODIFIER_PUNY2IDN))) {
        curl_free(*out);
        modifiers &= ~VARMODIFIER_PUNY2IDN;
        o->puny2idn = false;
        trurl_warnf(o,
                "Error converting url to IDN [%s]",
                curl_url_strerror(rc));
        return geturlpart(o, modifiers, uh, part, out);
    }
#endif
    return rc;
}

static void showurl(FILE *stream, struct option *o, int modifiers,
                    CURLU *uh)
{
  char *url;
  CURLUcode rc = geturlpart(o, modifiers, uh, CURLUPART_URL, &url);
  if(rc) {
    trurl_cleanup_options(o);
    verify(o, ERROR_BADURL, "invalid url [%s]", curl_url_strerror(rc));
    return;
  }
  fputs(url, stream);
  curl_free(url);
}

static void get(struct option *o, CURLU *uh)
{
  FILE *stream = stdout;
  const char *ptr = o->format;
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
        const char *start = ptr;
        char *end;
        char *cl;
        size_t vlen;
        bool isquery = false;
        bool queryall = false;
        int mods = 0;
        end = strchr(ptr, endbyte);
        ptr++; /* pass the { */
        if(!end) {
          /* syntax error */
          fputc(startbyte, stream);
          continue;
        }

        /* {path} {:path} */
        if(*ptr == ':') {
          mods |= VARMODIFIER_URLENCODED;
          ptr++;
        }
        vlen = end - ptr;
        do {
          cl = memchr(ptr, ':', vlen);
          if(!cl)
            break;

          /* modifiers! */
          if(!strncmp(ptr, "default:", cl - ptr + 1))
            mods |= VARMODIFIER_DEFAULT;
          else if(!strncmp(ptr, "puny:", cl - ptr + 1)) {
            if(mods & VARMODIFIER_PUNY2IDN)
              errorf(o, ERROR_GET,
                     "puny modifier is mutually exclusive with idn");
            mods |= VARMODIFIER_PUNY;
          }
          else if(!strncmp(ptr, "idn:", cl - ptr + 1)) {
            if(mods & VARMODIFIER_PUNY)
              errorf(o, ERROR_GET,
                     "idn modifier is mutually exclusive with puny");
            mods |= VARMODIFIER_PUNY2IDN;
          }
          else {
            /* {query: or {query-all: */
            if(!strncmp(ptr, "query-all:", cl - ptr + 1)) {
              isquery = true;
              queryall = true;
            }
            else if(!strncmp(ptr, "query:", cl - ptr + 1))
              isquery = true;
            else {
              /* syntax error */
              vlen = 0;
              end[1] = '\0';
            }
            break;
          }

          ptr = cl + 1;
          vlen = end - ptr;
        } while(true);

        if(isquery) {
          showqkey(stream, cl + 1, end - cl - 1,
                   !o->urlencode && !(mods & VARMODIFIER_URLENCODED),
                   queryall);
        }
        else if(!vlen)
          errorf(o, ERROR_GET, "Bad --get syntax: %s", start);
        else if(!strncmp(ptr, "url", vlen))
          showurl(stream, o, mods, uh);
        else {
          const struct var *v = comp2var(ptr, vlen);
          if(v) {
            char *nurl;
            CURLUcode rc = geturlpart(o, mods, uh, v->part, &nurl);
            switch(rc) {
            case CURLUE_OK:
              fputs(nurl, stream);
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
              trurl_warnf(o, "%s (%s)\n", curl_url_strerror(rc), v->name);
              break;
            }
          }
          else
            errorf(o, ERROR_GET, "\"%.*s\" is not a recognized URL component",
                   (int)vlen, ptr);
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

static const struct var *setone(CURLU *uh, const char *setline,
                                struct option *o)
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
      if((v->part == CURLUPART_HOST) && ('[' == ptr[1]))
        /* when setting an IPv6 numerical address, disable URL encoding */
        urlencode = false;
      rc = curl_url_set(uh, v->part, ptr[1] ? &ptr[1] : NULL,
                        (o->curl ? 0 : CURLU_NON_SUPPORT_SCHEME)|
                        (urlencode ? CURLU_URLENCODE : 0) );
      if(rc)
        warnf("Error setting %s: %s", v->name, curl_url_strerror(rc));
      found = true;
    }
    if(!found)
      errorf(o, ERROR_SET,
                   "unknown component: %.*s", (int)vlen, setline);
  }
  else
    errorf(o, ERROR_SET, "invalid --set syntax: %s", setline);
  return v;
}

static unsigned int set(CURLU *uh,
                        struct option *o)
{
  struct curl_slist *node;
  unsigned int mask = 0;
  for(node =  o->set_list; node; node = node->next) {
    const struct var *v;
    char *setline = node->data;
    v = setone(uh, setline, o);
    if(v) {
      if(mask & (1 << v->part))
        errorf(o, ERROR_SET,
                     "duplicate --set for component %s", v->name);
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
        fprintf(stream, "\\u%04x", *i);
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
  char *url;
  CURLUcode rc = geturlpart(o, 0, uh, CURLUPART_URL, &url);
  if(rc) {
    trurl_cleanup_options(o);
    verify(o, ERROR_BADURL, "invalid url [%s]", curl_url_strerror(rc));
    return;
  }
  printf("%s\n  {\n    \"url\": ", o->urls ? "," : "");
  jsonString(stdout, url, strlen(url), false);
  curl_free(url);
  fputs(",\n    \"parts\": {\n", stdout);
  for(i = 0; variables[i].name; i++) {
    char *part;
    rc = geturlpart(o, 0, uh, variables[i].part, &part);
    if(!rc) {
      if(!first)
        fputs(",\n", stdout);
      first = false;
      printf("      \"%s\": ", variables[i].name);
      jsonString(stdout, part, strlen(part), false);
      curl_free(part);
    }
  }
  fputs("\n    }", stdout);
  first = true;
  if(nqpairs) {
    int j;
    fputs(",\n    \"params\": [\n", stdout);
    for(j = 0 ; j < nqpairs; j++) {
      const char *sep = memchr(qpairsdec[j].str, '=', qpairsdec[j].len);
      const char *value = sep ? sep + 1 : "";
      int value_len = (int) qpairsdec[j].len - (int)(value - qpairsdec[j].str);
      /* don't print out empty/trimmed values */
      if(!qpairsdec[j].len || !qpairsdec[j].str[0])
        continue;
      if(!first)
        fputs(",\n", stdout);
      first = false;
      fputs("      {\n        \"key\": ", stdout);
      jsonString(stdout, qpairsdec[j].str,
                 sep ? (size_t)(sep - qpairsdec[j].str) :
                       qpairsdec[j].len,
                 false);
      fputs(",\n        \"value\": ", stdout);
      jsonString(stdout, sep?value:"", sep?value_len:0, false);
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
    char *ptr;
    char *instr = node->data;
    if(strncmp(instr, "query", 5))
      /* for now we can only trim query components */
      errorf(o, ERROR_TRIM, "Unsupported trim component: %s", instr);
    ptr = strchr(instr, '=');
    if(ptr && (ptr > instr)) {
      /* 'ptr' should be a fixed string or a pattern ending with an
         asterisk */
      size_t inslen;
      bool pattern = false;
      int i;
      char *temp = NULL;

      ptr++; /* pass the = */
      inslen = strlen(ptr);
      if(inslen) {
        pattern = ptr[inslen - 1] == '*';
        if(pattern && (inslen > 1)) {
          pattern ^= ptr[inslen - 2] == '\\';
          if(!pattern) {
            /* the two final letters are \*, but the backslash needs to be
               removed. Get a copy and edit that accordingly. */
            temp = strdup(ptr);
            if(!temp)
              return; /* out of memory, bail out */
            temp[inslen - 2] = '*';
            temp[inslen - 1] = '\0';
            ptr = temp;
            inslen--; /* one byte shorter now */
          }
        }
        if(pattern)
          inslen--;
      }

      for(i = 0 ; i < nqpairs; i++) {
        char *q = qpairs[i].str;
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
          free(qpairs[i].str);
          free(qpairsdec[i].str);
          qpairs[i].str = strdup(""); /* marked as deleted */
          qpairs[i].len = 0;
          qpairsdec[i].str = strdup(""); /* marked as deleted */
          qpairsdec[i].len = 0;
        }
      }
      free(temp);
    }
    query_is_modified = true;
  }
}

/* memdup the amount and add a trailing zero */
static struct string *memdupzero(char *source, size_t len)
{
  struct string *ret = malloc(sizeof(struct string));
  if(!ret)
    return NULL;
  ret->str = malloc(len + 1);
  if(!ret->str) {
    free(ret);
    return NULL;
  }
  memcpy(ret->str, source, len);
  ret->str[len] = 0;
  ret->len = len;
  return ret;
}

/* URL decode the pair and return it in an allocated chunk */
static struct string *memdupdec(char *source, size_t len, bool json)
{
  char *sep = memchr(source, '=', len);
  char *left = NULL;
  char *right = NULL;
  int right_len = 0;
  int left_len = 0;
  char *str;
  struct string *ret;
  left = strurldecode(source, (int)(sep ? (size_t)(sep - source) : len),
                      &left_len);
  if(sep) {
    char *p;
    int plen;
    right = strurldecode(sep + 1, (int)(len - (sep - source) - 1),
            &right_len);

    /* convert null bytes to periods */
    for(plen = right_len, p = right; plen; plen--, p++) {
      if(!*p && !json) {
        *p = REPLACE_NULL_BYTE;
      }
     }
  }
  str = malloc(sizeof(char) * (left_len + (sep?(right_len + 1):0)));
  if(!str)
    return NULL;
  memcpy(str, left, left_len);
  if(sep) {
    str[left_len] = '=';
    memcpy(str + 1 + left_len, right, right_len);
  }
  curl_free(right);
  curl_free(left);
  ret = malloc(sizeof(struct string));
  if(!ret) {
    return NULL;
  }
  ret->str = str;
  ret->len = left_len + (sep?(right_len + 1):0);
  return ret;
}


static void freeqpairs(void)
{
  int i;
  for(i = 0; i<nqpairs; i++) {
    if(qpairs[i].len) {
      free(qpairs[i].str);
      qpairs[i].str = NULL;
      free(qpairsdec[i].str);
      qpairsdec[i].str = NULL;
    }
  }
  nqpairs = 0;
}

/* store the pair both encoded and decoded */
static char *addqpair(char *pair, size_t len, bool json)
{
  struct string *p = NULL;
  struct string *pdec = NULL;
  char *ret = NULL;
  if(nqpairs < MAX_QPAIRS) {
    p = memdupzero(pair, len);
    pdec = memdupdec(pair, len, json);
    if(p && pdec) {
      qpairs[nqpairs].str = p->str;
      qpairs[nqpairs].len = p->len;
      qpairsdec[nqpairs].str = pdec->str;
      qpairsdec[nqpairs].len = pdec->len;
      nqpairs++;
    }
  }
  else
    warnf("too many query pairs");

  if(p)
    ret = p->str;
  if(pdec)
    free(pdec);
  if(p)
    free(p);
  return ret;
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
      addqpair(p, len, o->jsonout);
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
  if(query_is_modified) {
    int i;
    char *nq = NULL;
    for(i = 0; i<nqpairs; i++) {
      char *oldnq = nq;
      nq = curl_maprintf("%s%s%s", nq?nq:"",
                         (nq && *nq && *(qpairs[i].str))? o->qsep: "",
                         qpairs[i].str);
      curl_free(oldnq);
    }
    if(nq) {
      int rc = curl_url_set(uh, CURLUPART_QUERY, nq, 0);
      if(rc)
        trurl_warnf(o, "internal problem");
    }
    curl_free(nq);
  }
}

/* sort case insensitively */
static int cmpfunc(const void *p1, const void *p2)
{
  int i;
  int len = (int)((((struct string *)p1)->len) < (((struct string *)p2)->len)?
                  (((struct string *)p1)->len) : (((struct string *)p2)->len));

  for(i = 0; i < len; i++) {
    char c1 = ((struct string *)p1)->str[i] | ('a' - 'A');
    char c2 = ((struct string *)p2)->str[i] | ('a' - 'A');
    if(c1 != c2)
      return c1 - c2;
  }

  return 0;
}

static void sortquery(struct option *o)
{
  if(o->sort_query) {
    /* not these two lists may no longer be the same order after the sort */
    qsort(&qpairs[0], nqpairs, sizeof(struct string), cmpfunc);
    qsort(&qpairsdec[0], nqpairs, sizeof(struct string), cmpfunc);
    query_is_modified = true;
  }
}

static void replace(struct option *o)
{
  struct curl_slist *node;
  for(node = o->replace_list; node; node = node->next) {
    struct string key;
    struct string value;
    bool replaced = false;
    int i;
    key.str = node->data;
    value.str = strchr(key.str, '=');
    if(value.str) {
      key.len = value.str++ - key.str;
      value.len = strlen(value.str);
    }
    else {
      key.len = strlen(key.str);
      value.str = NULL;
      value.len = 0;
    }
    for(i = 0; i < nqpairs; i++) {
      char *q = qpairs[i].str;
      /* not the correct query, move on */
      if(strncmp(q, key.str, key.len))
        continue;
      free(qpairs[i].str);
      free(qpairsdec[i].str);
      /* this is a duplicate remove it. */
      if(replaced) {
        qpairs[i].len = 0;
        qpairs[i].str = strdup("");
        qpairsdec[i].len = 0;
        qpairsdec[i].str = strdup("");
        continue;
      }
      struct string *pdec =
        memdupdec(key.str, key.len + value.len + 1, o->jsonout);
      struct string *p = memdupzero(key.str, key.len + value.len + 1);
      qpairs[i].len = p->len;
      qpairs[i].str = p->str;
      qpairsdec[i].len = pdec->len;
      qpairsdec[i].str = pdec->str;
      free(pdec);
      free(p);
      replaced = true;
    }

    if(!replaced && o->force_replace) {
      trurl_warnf(o, "key '%.*s' not in url, appending to query",
                  (int) (key.len),
                  key.str);
      addqpair(key.str, strlen(key.str), o->jsonout);
    }
    query_is_modified = true;
  }
}
static CURLUcode seturl(struct option *o, CURLU *uh, const char *url)
{
  return curl_url_set(uh, CURLUPART_URL, url,
                      (o->no_guess_scheme ?
                       0 : CURLU_GUESS_SCHEME)|
                      (o->curl ? 0 : CURLU_NON_SUPPORT_SCHEME)|
                      (o->accept_space ?
                       CURLU_ALLOW_SPACE : 0)|
                      CURLU_URLENCODE);
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
      errorf(o, ERROR_MEM, "out of memory");
    if(url) {
      CURLUcode rc = seturl(o, uh, url);
      if(rc) {
        curl_url_cleanup(uh);
        verify(o, ERROR_BADURL, "%s [%s]", curl_url_strerror(rc), url);
        return;
      }
      if(o->redirect) {
        rc = seturl(o, uh, o->redirect);
        if(rc) {
          curl_url_cleanup(uh);
          verify(o, ERROR_BADURL, "invalid redirection: %s [%s]",
                 curl_url_strerror(rc), o->redirect);
          return;
        }
      }
    }
  }
  do {
    struct curl_slist *p;
    bool url_is_invalid = false;
    unsigned setmask = 0;

    /* set everything */
    setmask = set(uh, o);

    if(iter) {
      char iterbuf[1024];
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
          errorf(o, ERROR_ITER, "wrong iterate syntax");
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
        if(!v) {
          curl_url_cleanup(uh);
          errorf(o, ERROR_ITER, "bad component for iterate");
        }
        if(iinfo->varmask & (1<<v->part)) {
          curl_url_cleanup(uh);
          errorf(o, ERROR_ITER,
                       "duplicate component for iterate: %s", v->name);
        }
        if(setmask & (1 << v->part)) {
          curl_url_cleanup(uh);
          errorf(o, ERROR_ITER,
                 "duplicate --iterate and --set for component %s",
                 v->name);
        }
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
      setone(uh, iterbuf, o);
      if(iter->next) {
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

    /* trim parts */
    trim(o);

    /* replace parts */
    replace(o);

    /* append query segments */
    for(p = o->append_query; p; p = p->next) {
      addqpair(p->data, strlen(p->data), o->jsonout);
      query_is_modified = true;
    }

    sortquery(o);

    /* put the query back */
    qpair2query(uh, o);

    /* make sure the URL is still valid */
    if(!url || o->redirect || o->set_list || o->append_path) {
      char *ourl = NULL;
      CURLUcode rc = curl_url_get(uh, CURLUPART_URL, &ourl, 0);
      if(rc) {
        if(o->verify) /* only clean up if we're exiting */
          curl_url_cleanup(uh);
        verify(o, ERROR_URL, "not enough input for a URL");
        url_is_invalid = true;
      }
      else {
        rc = seturl(o, uh, ourl);
        if(rc) {
          if(o->verify) /* only clean up if we're exiting */
            curl_url_cleanup(uh);
          verify(o, ERROR_BADURL, "%s [%s]", curl_url_strerror(rc),
                 ourl);
          url_is_invalid = true;
        }
        else {
          char *nurl = NULL;
          rc = curl_url_get(uh, CURLUPART_URL, &nurl, 0);
          if(!rc)
            curl_free(nurl);
          else {
            if(o->verify) /* only clean up if we're exiting */
              curl_url_cleanup(uh);
            verify(o, ERROR_BADURL, "url became invalid");
            url_is_invalid = true;
          }
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
      int rc = geturlpart(o, 0, uh, CURLUPART_URL, &nurl);
      if(!rc) {
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
        errorf(&o, ERROR_FLAG, "unknown option: %s", argv[0]);
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
    putchar('[');

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
        trurl_warnf(&o, "skipping long line");
        do {
          ch = getc(o.url);
        } while(ch != EOF && ch != '\n');
        if(ch == EOF) {
          if(ferror(o.url))
            trurl_warnf(&o, "getc: %s", strerror(errno));
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
      trurl_warnf(&o, "fgets: %s", strerror(errno));
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
    printf("%s]\n", o.urls ? "\n" : "");
  /* we're done with libcurl, so clean it up */
  trurl_cleanup_options(&o);
  curl_global_cleanup();
  return exit_status;
}
