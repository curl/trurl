
# [![trurl logo](https://curl.se/logo/trurl-logo.svg)](https://curl.se/trurl)

# trurl

Command line tool for URL parsing and manipulation

[Video presentation](https://youtu.be/oDL7DVszr2w)

## Examples

**Replace the hostname of a URL:**

```text
$ trurl --url https://curl.se --set host=example.com
https://example.com/
```

**Create a URL by setting components:**

```text
$ trurl --set host=example.com --set scheme=ftp
ftp://example.com/
```

**Redirect a URL:**

```text
$ trurl --url https://curl.se/we/are.html --redirect here.html
https://curl.se/we/here.html
```

**Change port number:**

```text
$ trurl --url https://curl.se/we/../are.html --set port=8080
https://curl.se:8080/are.html
```

**Extract the path from a URL:**

```text
$ trurl --url https://curl.se/we/are.html --get '{path}'
/we/are.html
```

**Extract the port from a URL:**

```text
$ trurl --url https://curl.se/we/are.html --get '{port}'
443
```

**Append a path segment to a URL:**

```text
$ trurl --url https://curl.se/hello --append path=you
https://curl.se/hello/you
```

**Append a query segment to a URL:**

```text
$ trurl --url "https://curl.se?name=hello" --append query=search=string
https://curl.se/?name=hello&search=string
```

**Read URLs from stdin:**

```text
$ cat urllist.txt | trurl --url-file -
...
```

**Output JSON:**

```text
$ trurl "https://fake.host/hello#frag" --set user=::moo:: --json
[
  {
    "url": "https://%3a%3amoo%3a%3a@fake.host/hello#frag",
    "parts": {
      "scheme": "https",
      "user": "::moo::",
      "host": "fake.host",
      "path": "/hello",
      "fragment": "frag"
    }
  }
]
```

**Remove tracking tuples from query:**

```text
$ trurl "https://curl.se?search=hey&utm_source=tracker" --qtrim "utm_*"
https://curl.se/?search=hey
```

**Show a specific query key value:**

```text
$ trurl "https://example.com?a=home&here=now&thisthen" -g '{query:a}'
home
```

**Sort the key/value pairs in the query component:**

```text
$ trurl "https://example.com?b=a&c=b&a=c" --sort-query
https://example.com?a=c&b=a&c=b
```

**Work with a query that uses a semicolon separator:**

```text
$ trurl "https://curl.se?search=fool;page=5" --qtrim "search" --query-separator ";"
https://curl.se?page=5
```

**Accept spaces in the URL path:**

```text
$ trurl "https://curl.se/this has space/index.html" --accept-space
https://curl.se/this%20has%20space/index.html
```

## Install

### Linux

It is quite easy to compile the C source with GCC:

```text
$ make
cc  -W -Wall -pedantic -g   -c -o trurl.o trurl.c
cc   trurl.o  -lcurl -o trurl
```

trurl is also available in [some package managers](https://github.com/curl/trurl/wiki/Get-trurl-for-your-OS). If it is not listed you can try searching for it using the package manager of your preferred distribution.

### Windows

1. Download and run [Cygwin installer.](https://www.cygwin.com/install.html)
2. Follow the instructions provided by the installer. When prompted to select packages, make sure to choose the following: curl, libcurl-devel, libcurl4, make and gcc-core.
3. (optional) Add the Cygwin bin directory to your system PATH variable.
4. Use `make`, just like on Linux.

## Prerequisites

Development files of libcurl (e.g. `libcurl4-openssl-dev` or
`libcurl4-gnutls-dev`) are needed for compilation. Requires libcurl version
7.62.0 or newer (the first libcurl to ship the URL parsing API).

trurl also uses `CURLUPART_ZONEID` added in libcurl 7.81.0 and
`curl_url_strerror()` added in libcurl 7.80.0

It would certainly be possible to make trurl work with older libcurl versions
if someone wanted to.

### Older libcurls

trurl builds with libcurl older than 7.81.0 but will then not work as
good. For all the documented goodness, use a more modern libcurl.

### trurl / libcurl Compatibility

| trurl Feature   |  Minimum libcurl Version |
|-----------------|--------------------------|
| imap-options    |   7.30.0                 |
| normalize-ipv   |   7.77.0                 | 
| white-space     |   7.78.0                 |
| url-strerror    |   7.80.0                 |
| zone-id         |   7.81.0                 |
| punycode        |   7.88.0                 |
| punycode2idn    |   8.3.0                  |
| no-guess-scheme |   8.9.0                  |

For more details on how trurl will behave if these features are missing see [URL Quirks](https://github.com/curl/trurl/blob/master/URL-QUIRKS.md).
To see the features your version of trurl supports as well as the version of libcurl it is built with, run the following command:  `trurl --version`

