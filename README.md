# urler
command line tool for URL parsing and manipulation

[original idea](https://curl.se/mail/archive-2023-03/0030.html)

## Example command lines

~~~
  $ urler --url https://curl.se --host example.com
  https://example.com/

  $ urler --host example.com --scheme ftp
  ftp://example.com

  $ urler --url https://curl.se/we/are.html --redirect here.html
  https://curl.se/we/here.html

  $ urler --url https://curl.se/we/../are.html --port 8080
  https://curl.se:8080/are.html

  $ urler --url https://curl.se/we/are.html --only-path
  /we/are.html

  $ urler --url https://curl.se/we/are.html --only-port
  443
~~~

## Install

On Linux :

It's quite easy to compile the C source with GCC :

```
$ make
cc  -W -Wall -pedantic -g   -c -o urler.o urler.c
cc   urler.o  -lcurl -o urler
```