---
c: Copyright (C) Daniel Stenberg, <daniel.se>, et al.
SPDX-License-Identifier: curl
Title: trurl
Section: 1
Source: trurl 0.16
See-also:
  - curl (1)
  - wcurl (1)
---

# NAME

trurl - transpose URLs

# SYNOPSIS

**trurl [options / URLs]**

# DESCRIPTION

**trurl** parses, manipulates and outputs URLs and parts of URLs.

It uses the RFC 3986 definition of URLs and it uses libcurl's URL parser to do
so, which includes a few "extensions". The URL support is limited to
"hierarchical" URLs, the ones that use `://` separators after the scheme.

Typically you pass in one or more URLs and decide what of that you want
output. Possibly modifying the URL as well.

trurl knows URLs and every URL consists of up to ten separate and independent
*components*. These components can be extracted, removed and updated with
trurl and they are referred to by their respective names: scheme, user,
password, options, host, port, path, query, fragment and zoneid.

# NORMALIZATION

When provided a URL to work with, trurl "normalizes" it. It means that
individual URL components are URL decoded then URL encoded back again and set
in the URL.

Example:

    $ trurl 'http://ex%61mple:80/%62ath/a/../b?%2e%FF#tes%74'
    http://example/bath/b?.%ff#test

# OPTIONS

Options start with one or two dashes. Many of the options require an
additional value next to them.

Any other argument is interpreted as a URL argument, and is treated as if it
was following a `--url` option.

The first argument that is exactly two dashes (`--`), marks the end of
options; any argument after the end of options is interpreted as a URL
argument even if it starts with a dash.

Long options can be provided either as `--flag argument` or as
`--flag=argument`.

## -a, --append [component]=[data]

Append data to a component. This can only append data to the path and the
query components.

For path, this URL encodes and appends the new segment to the path, separated
with a slash.

For query, this URL encodes and appends the new segment to the query,
separated with an ampersand (&). If the appended segment contains an equal
sign (`=`) that one is kept verbatim and both sides of the first occurrence
are URL encoded separately.

## --accept-space

When set, trurl tries to accept spaces as part of the URL and instead URL
encode such occurrences accordingly.

According to RFC 3986, a space cannot legally be part of a URL. This option
provides a best-effort to convert the provided string into a valid URL.

## --as-idn

Converts a punycode ASCII hostname to its original International Domain Name
in Unicode. If the hostname is not using punycode then the original hostname
is used.

## --curl

Only accept URL schemes supported by libcurl.

## --default-port

When set, trurl uses the scheme's default port number for URLs with a known
scheme, and without an explicit port number.

Note that trurl only knows default port numbers for URL schemes that are
supported by libcurl.

Since, by default, trurl removes default port numbers from URLs with a known
scheme, this option is pretty much ignored unless one of *--get*, *--json*,
and *--keep-port* is not also specified.

## -f, --url-file [filename]

Read URLs to work on from the given file. Use the filename `-` (a single
minus) to tell trurl to read the URLs from stdin.

Each line needs to be a single valid URL. trurl removes one carriage return
character at the end of the line if present, trims off all the trailing space
and tab characters, and skips all empty (after trimming) lines.

The maximum line length supported in a file like this is 4094 bytes. Lines
that exceed that length are skipped, and a warning is printed to stderr when
they are encountered.

## -g, --get [format]

Output text and URL data according to the provided format string. Components
from the URL can be output when specified as **{component}** or
**[component]**, with the name of the part show within curly braces or
brackets. You can not mix braces and brackets for this purpose in the same
command line.

The following component names are available (case sensitive): url, scheme,
user, password, options, host, port, path, query, fragment and zoneid.

**{component}** expands to nothing if the given component does not have a
value.

Components are shown URL decoded by default.

URL decoding a component may cause problems to display it. Such problems make
a warning get displayed unless **--quiet** is used.

trurl supports a range of different qualifiers, or prefixes, to the component
that changes how it handles it:

If **url:** is specified, like `{url:path}`, the component gets output URL
encoded. As a shortcut, `url:` also works written as a single colon:
`{:path}`.

If **strict:** is specified, like `{strict:path}`, URL decode problems are
turned into errors. In this stricter mode, a URL decode problem makes trurl
stop what it is doing and return with exit code 10.

If **must:** is specified, like `{must:query}`, it makes trurl return an error
if the requested component does not exist in the URL. By default a missing
component will just be shown blank.

If **default:** is specified, like `{default:url}` or `{default:port}`, and
the port is not explicitly specified in the URL, the scheme's default port is
output if it is known.

If **puny:** is specified, like `{puny:url}` or `{puny:host}`, the punycoded
version of the hostname is used in the output. This option is mutually
exclusive with **idn:**.

If **idn:** is specified like `{idn:url}` or `{idn:host}`, the International
Domain Name version of the hostname is used in the output if it is provided
as a correctly encoded punycode version. This option is mutually exclusive
with **puny:**.

If *--default-port* is specified, all formats are expanded as if they used
*default:*; and if *--punycode* is used, all formats are expanded as if they
used *puny:*. Also note that `{url}` is affected by the *--keep-port* option.

Hosts provided as IPv6 numerical addresses are provided within square
brackets. Like `[fe80::20c:29ff:fe9c:409b]`.

Hosts provided as IPv4 numerical addresses are *normalized* and provided as
four dot-separated decimal numbers when output.

You can access specific keys in the query string using the format
**{query:key}**. Then the value of the first matching key is output using a
case sensitive match. When extracting a URL decoded query key that contains
`%00`, such octet is replaced with a single period `.` in the output.

You can access specific keys in the query string and out all values using the
format **{query-all:key}**. This looks for *key* case sensitively and outputs
all values for that key space-separated.

The *format* string supports the following backslash sequences:

\\ - backslash

\\t - tab

\\n - newline

\\r - carriage return

\\{ - an open curly brace that does not start a variable

\\[ - an open bracket that does not start a variable

All other text in the format string is shown as-is.

## -h, --help

Show the help output.

## --iterate [component]=[item1 item2 ...]

Set the component to multiple values and output the result once for each
iteration. Several combined iterations are allowed to generate combinations,
but only one *--iterate* option per component. The listed items to iterate
over should be separated by single spaces.

Example:

    $ trurl example.com --iterate=scheme="ftp https" --iterate=port="22 80"
    ftp://example.com:22/
    ftp://example.com:80/
    https://example.com:22/
    https://example.com:80/

## --json

Outputs all set components of the URLs as JSON objects. All components of the
URL that have data get populated in the parts object using their component
names. See below for details on the format.

The URL components are provided URL decoded. Change that with **--urlencode**.

## --keep-port

By default, trurl removes default port numbers from URLs with a known scheme
even if they are explicitly specified in the input URL. This options, makes
trurl not remove them.

Example:

    $ trurl https://example.com:443/ --keep-port
    https://example.com:443/

## --no-guess-scheme

Disables libcurl's scheme guessing feature. URLs that do not contain a scheme
are treated as invalid URLs.

Example:

    $ trurl example.com --no-guess-scheme
    trurl note: Bad scheme [example.com]

## --punycode

Uses the punycode version of the hostname, which is how International Domain
Names are converted into plain ASCII. If the hostname is not using IDN, the
regular ASCII name is used.

Example:

    $ trurl http://åäö/ --punycode
    http://xn--4cab6c/

## --qtrim [what]

Trims data off a query.

*what* is specified as a full name of a name/value pair, or as a word prefix
(using a single trailing asterisk (`*`)) which makes trurl remove the tuples
from the query string that match the instruction.

To match a literal trailing asterisk instead of using a wildcard, escape it
with a backslash in front of it. Like `\\*`.

## --query-separator [what]

Specify the single letter used for separating query pairs. The default is `&`
but at least in the past sometimes semicolons `;` or even colons `:` have been
used for this purpose. If your URL uses something other than the default
letter, setting the right one makes sure trurl can do its query operations
properly.

Example:

    $ trurl "https://curl.se?b=name:a=age" --sort-query --query-separator ":"
    https://curl.se/?a=age:b=name

## --quiet

Suppress (some) notes and warnings.

## --redirect [URL]

Redirect the URL to this new location. The redirection is performed on the
base URL, so, if no base URL is specified, no redirection is performed.

Example:

    $ trurl --url https://curl.se/we/are.html --redirect ../here.html
    https://curl.se/here.html

## --replace [data]

Replaces a URL query.

data can either take the form of a single value, or as a key/value pair in the
shape *foo=bar*. If replace is called on an item that is not in the list of
queries trurl ignores that item.

trurl URL encodes both sides of the `=` character in the given input data
argument.

## --replace-append [data]

Works the same as *--replace*, but trurl appends a missing query string if
it is not in the query list already.

## -s, --set [component][:]=[data]

Set this URL component. Setting blank string (`""`) clears the component from
the URL.

The following components can be set: url, scheme, user, password, options,
host, port, path, query, fragment and zoneid.

If a simple `=`-assignment is used, the data is URL encoded when applied. If
`:=` is used, the data is assumed to already be URL encoded and stored as-is.

If `?=` is used, the set is only performed if the component is not already
set. It avoids overwriting any already set data.

You can also combine `:` and `?` into `?:=` if desired.

If no URL or *--url-file* argument is provided, trurl tries to create a URL
using the components provided by the *--set* options. If not enough components
are specified, this fails.

## --sort-query

The "variable=content" tuplets in the query component are sorted in a case
insensitive alphabetical order. This helps making URLs identical that
otherwise only had their query pairs in different orders.

## --trim [component]=[what]

Deprecated: use **--qtrim**.

Trims data off a component. Currently this can only trim a query component.

*what* is specified as a full word or as a word prefix (using a single
trailing asterisk (`*`)) which makes trurl remove the tuples from the query
string that match the instruction.

To match a literal trailing asterisk instead of using a wildcard, escape it
with a backslash in front of it. Like `\\*`.

## --url [URL]

Set the input URL to work with. The URL may be provided without a scheme,
which then typically is not actually a legal URL but trurl tries to figure
out what is meant and guess what scheme to use (unless *--no-guess-scheme*
is used).

Providing multiple URLs makes trurl act on all URLs in a serial fashion.

If the URL cannot be parsed for whatever reason, trurl simply moves on to
the next provided URL - unless *--verify* is used.

## --urlencode

Outputs URL encoded version of components by default when using *--get* or
*--json*.

## -v, --version

Show version information and exit.

## --verify

When a URL is provided, return error immediately if it does not parse as a
valid URL. In normal cases, trurl can forgive a bad URL input.

# URL COMPONENTS

## scheme

This is the leading character sequence of a URL, excluding the "://"
separator. It cannot be specified URL encoded.

A URL cannot exist without a scheme, but unless **--no-guess-scheme** is used
trurl guesses what scheme that was intended if none was provided.

Examples:

    $ trurl https://odd/ -g '{scheme}'
    https

    $ trurl odd -g '{scheme}'
    http

    $ trurl odd -g '{scheme}' --no-guess-scheme
    trurl note: Bad scheme [odd]

## user

After the scheme separator, there can be a username provided. If it ends with
a colon (`:`), there is a password provided. If it ends with an at character
(`@`) there is no password provided in the URL.

Example:

    $ trurl https://user%3a%40:secret@odd/ -g '{user}'
    user:@

## password

If the password ends with a semicolon (`;`) there is an options field
following. This field is only accepted by trurl for URLs using the IMAP
scheme.

Example:

    $ trurl https://user:secr%65t@odd/ -g '{password}'
    secret

## options

This field can only end with an at character (`@`) that separates the options
from the hostname.

    $ trurl 'imap://user:pwd;giraffe@odd' -g '{options}'
    giraffe

If the scheme is not IMAP, the `giraffe` part is instead considered part of
the password:

    $ trurl 'sftp://user:pwd;giraffe@odd' -g '{password}'
    pwd;giraffe

We strongly advice users to %-encode `;`, `:` and `@` in URLs of course to
reduce the risk for confusions.

## host

The host component is the hostname or a numerical IP address. If a hostname is
provided, it can be an International Domain Name non-ASCII characters. A
hostname can be provided URL encoded.

trurl provides options for working with the IDN hostnames either as IDN or in
its punycode version.

Example, convert an IDN name to punycode in the output:

    $ trurl http://åäö/ --punycode
    http://xn--4cab6c/

Or the reverse, convert a punycode hostname into its IDN version:

    $ trurl http://xn--4cab6c/ --as-idn
    http://åäö/

If the URL's hostname starts with an open bracket (`[`) it is a numerical IPv6
address that also must end with a closing bracket (`]`). trurl normalizes IPv6
addreses.

Example:

    $ trurl 'http://[2001:9b1:0:0:0:0:7b97:364b]/'
    http://[2001:9b1::7b97:364b]/

A numerical IPV4 address can be specified using one, two, three or four
numbers separated with dots and they can use decimal, octal or hexadecimal.
trurl normalizes provided addresses and uses four dotted decimal numbers in
its output.

Examples:

    $ trurl http://646464646/
    http://38.136.68.134/

    $ trurl http://246.646/
    http://246.0.2.134/

    $ trurl http://246.46.646/
    http://246.46.2.134/

    $ trurl http://0x14.0xb3022/
    http://20.11.48.34/

## zoneid

If the provided host is an IPv6 address, it might contain a specific zoneid. A
number or a network interface name normally.

Example:

    $ trurl 'http://[2001:9b1::f358:1ba4:7b97:364b%enp3s0]/' -g '{zoneid}'
    enp3s0

## port

If the host ends with a colon (`:`) then a port number follows. It is a 16 bit
decimal number that may not be URL encoded.

trurl knows the default port number for many URL schemes so it can show port
numbers for a URL even if none was explicitly used in the URL. With
**--default-port** it can add the default port to a URL even when not provide.

Example:

    $ trurl http:/a --default-port
    http://a:80/

Similarly, trurl normally hides the port number if the given number is the
default.

Example:

    $ trurl http:/a:80
    http://a/

But a user can make trurl keep the port even if it is the default, with
**--keep-port**.

Example:

    $ trurl http:/a:80 --keep-port
    http://a:80/

## path

A URL path is assumed to always start with and contain at least a slash (`/`),
even if none is actually provided in the URL.

Example:

    $ trurl http://xn--4cab6c -g '[path]'
    /

When setting the path, trurl will inject a leading slash if none is provided:

    $ trurl http://hello -s path="pony"
    http://hello/pony

    $ trurl http://hello -s path="/pony"
    http://hello/pony

If the input path contains dotdot or dot-slash sequences, they are normalized
away.

Example:

    $ trurl http://hej/one/../two/../three/./four
    http://hej/three/four

You can append a new segment to an existing path with **--append** like this:

    $ trurl http://twelve/three?hello --append path=four
    http://twelve/three/four?hello

## query

The query part does not include the leading question mark (`?`) separator when
extracted with trurl.

Example:

    $ trurl http://horse?elephant -g '{query}'
    elephant

Example, if you set the query with a leading question mark:

    $ trurl http://horse?elephant -s "query=?elephant"
    http://horse/?%3felephant

Query parts are often made up of a series of name=value pairs separated with
ampersands (`&`), and trurl offers several ways to work with such.

Append a new name value pair to a URL with **--append**:

    $ trurl http://host?name=hello --append query=search=life
    http://host/?name=hello&search=life

You cam **--replace** the value of a specific existing name among the pairs:

    $ trurl 'http://alpha?one=real&two=fake' --replace two=alsoreal
    http://alpha/?one=real&two=alsoreal

If the specific name you want to replace perhaps does not exist in the URL,
you can opt to replace *or* append the pair:

    $ trurl 'http://alpha?one=real&two=fake' --replace-append three=alsoreal
    http://alpha/?one=real&two=fake&three=alsoreal

In order to perhaps compare two URLs using query name value pairs, sorting
them first at least increases the chances of it working:

    $ trurl "http://alpha/?one=real&two=fake&three=alsoreal" --sort-query
    http://alpha/?one=real&three=alsoreal&two=fake

Remove name/value pairs from the URL by specifying exact name or wildcard
pattern with **--qtrim**:

    $ trurl 'https://example.com?a12=hej&a23=moo&b12=foo' --qtrim a*'
    https://example.com/?b12=foo

## fragment

The fragment part does not include the leading hash sign (`#`) separator when
extracted with trurl.

Example:

    $ trurl http://horse#elephant -g '{fragment}'
    elephant

Example, if you set the fragment with a leading hash sign:

    $ trurl "http://horse#elephant" -s "fragment=#zebra"
    http://horse/#%23zebra

The fragment part of a URL is for local purposes only. The data in there is
never actually sent over the network when a URL is used for transfers.

## url

trurl supports **url** as a named component for **--get** to allow for more
powerful outputs, but of course it is not actually a "component"; it is the
full URL.

Example:

    $ trurl ftps://example.com:2021/p%61th -g '{url}'
    ftps://example.com:2021/path

# JSON output format

The *--json* option outputs a JSON array with one or more objects. One for
each URL. Each URL JSON object contains a number of properties, a series of
key/value pairs. The exact set present depends on the given URL.

## url

This key exists in every object. It is the complete URL. Affected by
*--default-port*, *--keep-port*, and *--punycode*.

## parts

This key exists in every object, and contains an object with a key for each of
the settable URL components. If a component is missing, it means it is not
present in the URL. The parts are URL decoded unless *--urlencode* is used.

## parts.scheme
The URL scheme.

## parts.user
The username.

## parts.password
The password.

## parts.options
The options. Note that only a few URL schemes support the "options"
component.

## parts.host
The normalized hostname. It might be a UTF-8 name if an IDN name was used. It
can also be a normalized IPv4 or IPv6 address. An IPv6 address always starts
with a bracket (**[**) - and no other hostnames can contain such a symbol. If
*--punycode* is used, the punycode version of the host is outputted instead.

## parts.port
The provided port number as a string. If the port number was not provided in
the URL, but the scheme is a known one, and *--default-port* is in use, the
default port for that scheme is provided here.

## parts.path
The path. Including the leading slash.

## parts.query
The full query, excluding the question mark separator.

## parts.fragment
The fragment, excluding the pound sign separator.

## parts.zoneid
The zone id, which can only be present in an IPv6 address. When this key is
present, then **host** is an IPv6 numerical address.

## params

This key contains an array of query key/value objects. Each such pair is
listed with "key" and "value" and their respective contents in the output.

The key/values are extracted from the query where they are separated by
ampersands (**&**) - or the user sets with **--query-separator**.

The query pairs are listed in the order of appearance in a left-to-right
order, but can be made alpha-sorted with **--sort-query**.

It is only present if the URL has a query.

# EXAMPLES

## Replace the hostname of a URL

~~~
$ trurl --url https://curl.se --set host=example.com
https://example.com/
~~~

## Create a URL by setting components

~~~
 $ trurl --set host=example.com --set scheme=ftp
 ftp://example.com/
~~~

## Redirect a URL

~~~
$ trurl --url https://curl.se/we/are.html --redirect here.html
https://curl.se/we/here.html
~~~

## Change port number

This also shows how trurl removes dot-dot sequences
~~~
$ trurl --url https://curl.se/we/../are.html --set port=8080
https://curl.se:8080/are.html
~~~

## Extract the path from a URL

~~~
$ trurl --url https://curl.se/we/are.html --get '{path}'
/we/are.html
~~~

## Extract the port from a URL

This gets the default port based on the scheme if the port is not set in the
URL.
~~~
$ trurl --url https://curl.se/we/are.html --get '{default:port}'
443
~~~

## Append a path segment to a URL

~~~
$ trurl --url https://curl.se/hello --append path=you
https://curl.se/hello/you
~~~

## Append a query segment to a URL

~~~
$ trurl --url "https://curl.se?name=hello" --append query=search=string
 https://curl.se/?name=hello&search=string
~~~

## Read URLs from stdin

~~~
$ cat urllist.txt | trurl --url-file -
\&...
~~~

## Output JSON

~~~
$ trurl "https://fake.host/search?q=answers&user=me#frag" --json
[
  {
    "url": "https://fake.host/search?q=answers&user=me#frag",
    "parts": [
        "scheme": "https",
        "host": "fake.host",
        "path": "/search",
        "query": "q=answers&user=me"
        "fragment": "frag",
    ],
    "params": [
      {
        "key": "q",
        "value": "answers"
      },
      {
        "key": "user",
        "value": "me"
      }
    ]
  }
]
~~~

## Remove tracking tuples from query

~~~
$ trurl "https://curl.se?search=hey&utm_source=tracker" --qtrim "utm_*"
https://curl.se/?search=hey
~~~

## Show a specific query key value

~~~
$ trurl "https://example.com?a=home&here=now&thisthen" -g '{query:a}'
home
~~~

## Sort the key/value pairs in the query component

~~~
$ trurl "https://example.com?b=a&c=b&a=c" --sort-query
https://example.com?a=c&b=a&c=b
~~~

## Work with a query that uses a semicolon separator

~~~
$ trurl "https://curl.se?search=fool;page=5" --qtrim "search" --query-separator ";"
https://curl.se?page=5
~~~

## Accept spaces in the URL path

~~~
$ trurl "https://curl.se/this has space/index.html" --accept-space
https://curl.se/this%20has%20space/index.html
~~~

## Create multiple variations of a URL with different schemes

~~~
$ trurl "https://curl.se/path/index.html" --iterate "scheme=http ftp sftp"
http://curl.se/path/index.html
ftp://curl.se/path/index.html
sftp://curl.se/path/index.html
~~~

# EXIT CODES

trurl returns a non-zero exit code to indicate problems.

## 1

A problem with --url-file

## 2

A problem with --append

## 3

A command line option misses an argument

## 4

A command line option mistake or an illegal option combination.

## 5

A problem with --set

## 6

Out of memory

## 7

Could not output a valid URL

## 8

A problem with --qtrim

## 9

If --verify is set and the input URL cannot parse.

## 10

A problem with --get

## 11

A problem with --iterate

## 12

A problem with --replace or --replace-append

# WWW

https://curl.se/trurl
