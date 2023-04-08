# trurl

command line tool for URL parsing and manipulation

[video presentation](https://youtu.be/oDL7DVszr2w)

## Example command lines

~~~
  $ trurl --url https://curl.se --set host=example.com
  https://example.com/

  $ trurl --set host=example.com --set scheme=ftp
  ftp://example.com/

  $ trurl --url https://curl.se/we/are.html --redirect here.html
  https://curl.se/we/here.html

  $ trurl --url https://curl.se/we/../are.html --set port=8080
  https://curl.se:8080/are.html

  $ trurl --url https://curl.se/we/are.html --get '{path}'
  /we/are.html

  $ trurl --url https://curl.se/we/are.html --get '{port}'
  443

  $ trurl https://example.com/hello.html --get '{scheme} {port} {path}'
  https 443 /hello.html

  $ trurl --url https://curl.se/hello --append path=you
  https://curl.se/hello/you

  $ trurl --url "https://curl.se?name=hello" --append query=search=string
  https://curl.se/?name=hello&search=string

  $ trurl --url-file url-list.txt --get '{host}'
  [one host name per URL in the input file]

  $ cat url-list.txt | trurl --url-file - --get '{host}'
  [one host name per URL in the input file]

  $ trurl "https://fake.host/hello#frag" --set user=::moo:: --json
  [
    {
      "url": "https://%3a%3amoo%3a%3a@fake.host/hello#frag",
      "scheme": "https",
      "user": "::moo::",
      "host": "fake.host",
      "port": "443",
      "path": "/hello",
      "fragment": "frag"
    }
  ]

  $ trurl "https://example.com?search=hello&utm_source=tracker" --trim query="utm_*"
  https://example.com/?search=hello
~~~

## Install

**On Linux :**

It's quite easy to compile the C source with GCC :

```
$ make
cc  -W -Wall -pedantic -g   -c -o trurl.o trurl.c
cc   trurl.o  -lcurl -o trurl
```

**On Windows:**
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
