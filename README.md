# trurl

command line tool for URL parsing and manipulation

[original idea](https://curl.se/mail/archive-2023-03/0030.html)

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

~~~

## Install

On Linux :

It's quite easy to compile the C source with GCC :

```
$ make
cc  -W -Wall -pedantic -g   -c -o trurl.o trurl.c
cc   trurl.o  -lcurl -o trurl
```

Note that development files of libcurl (e.g. `libcurl4-openssl-dev` or
`libcurl4-gnutls-dev`) are needed for compilation. Requries libcurl version
7.62.0 or newer.
