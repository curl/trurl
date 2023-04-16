#!/usr/bin/perl

use strict;
use warnings;
use 5.13.9; # minimum version for JSON::PP

use Test::More;
use JSON::PP;

my @t = (
    "example.com|http://example.com/",
    "http://example.com|http://example.com/",
    "https://example.com|https://example.com/",
    "hp://example.com|hp://example.com/",
    "|",
    "ftp.example.com|ftp://ftp.example.com/",
    "https://example.com/../moo|https://example.com/moo",
    "https://example.com/.././moo|https://example.com/moo",
    "https://example.com/test/../moo|https://example.com/moo",
    "localhost --append path=moo|http://localhost/moo",
    "localhost -a path=moo|http://localhost/moo",
    "--set host=moo --set scheme=http|http://moo/",
    "-s host=moo -s scheme=http|http://moo/",
    "--set host=moo --set scheme=https --set port=999|https://moo:999/",
    "--set host=moo --set scheme=ftps --set path=/hello|ftps://moo/hello",
    "--url https://curl.se --set host=example.com|https://example.com/",
    "--set host=example.com --set scheme=ftp|ftp://example.com/",
    "--url https://curl.se/we/are.html --redirect here.html|https://curl.se/we/here.html",
    "--url https://curl.se/we/../are.html --set port=8080|https://curl.se:8080/are.html",
    "https://curl.se:22/ -s port=443|https://curl.se/",
    "https://curl.se:22/ -s port=443 --get {url}|https://curl.se/",
    "--url https://curl.se/we/are.html --get \"{path}\"|/we/are.html",
    "--url https://curl.se/we/are.html --get \"{port}\"|443",
    "--url https://curl.se/we/are.html --get \"{scheme}\"|https",
    "--url https://hello\@curl.se/we/are.html --get \"{user}\"|hello",
    "--url https://hello:secret\@curl.se/we/are.html --get \"{password}\"|secret",
    "--url \"imap://hello:secret;crazy\@curl.se/we/are.html\" --get \"{options}\"|crazy",
    "--url https://curl.se/we/are.html --get \"{host}\"|curl.se",
    "--url https://10.1/we/are.html --get \"{host}\"|10.0.0.1",
    "--url https://[fe80::0000:20c:29ff:fe9c:409b]:8080/we/are.html --get \"{host}\"|[fe80::20c:29ff:fe9c:409b]",
    "--url https://[fe80::0000:20c:29ff:fe9c:409b%euth0]:8080/we/are.html --get \"{zoneid}\"|euth0",
    "--url https://[fe80::0000:20c:29ff:fe9c:409b%eth0]:8080/we/are.html --get \"{zoneid}\"|eth0",
    "--url \"https://curl.se/we/are.html?user=many#more\" --get \"{query}\"|user=many",
    "--url \"https://curl.se/we/are.html?user=many#more\" --get \"{fragment}\"|more",
    "--url https://curl.se/we/are.html -g \"{port}\"|443",
    "--url https://curl.se/hello --append path=you|https://curl.se/hello/you",
    "--url https://curl.se/hello --append \"path=you index.html\"|https://curl.se/hello/you%20index.html",
    "--url \"https://curl.se?name=hello\" --append query=search=string|https://curl.se/?name=hello&search=string",
    "--url https://curl.se/hello --set user=:hej:|https://%3ahej%3a\@curl.se/hello",
    "--url https://curl.se/hello --set user=hej --set password=secret|https://hej:secret\@curl.se/hello",
    "--url https://curl.se/hello --set query:=user=me|https://curl.se/hello?user=me",
    "--url https://curl.se/hello --set query=user=me|https://curl.se/hello?user%3dme",
    "--url https://curl.se/hello --set fragment=\" hello\"|https://curl.se/hello#%20hello",
    "--url https://curl.se/hello --set fragment:=\"%20hello\"|https://curl.se/hello#%20hello",
    "localhost --append query=hello=foo|http://localhost/?hello=foo",
    "localhost -a query=hello=foo|http://localhost/?hello=foo",
    "\"https://example.com?search=hello&utm_source=tracker\" --trim query=\"utm_*\"|https://example.com/?search=hello",
    "\"https://example.com?search=hello&utm_source=tracker&more=data\" --trim query=\"utm_*\"|https://example.com/?search=hello&more=data",
    "\"https://example.com?search=hello&more=data\" --trim query=\"utm_*\"|https://example.com/?search=hello&more=data",
    "\"https://example.com?utm_source=tracker\" --trim query=\"utm_*\"|https://example.com/",
    "\"https://example.com?search=hello&utm_source=tracker&more=data\" --trim query=\"utm_source\"|https://example.com/?search=hello&more=data",
    "\"https://example.com?search=hello&utm_source=tracker&more=data\" --trim query=\"utm_source\" --trim query=more --trim query=search|https://example.com/",
    "--accept-space --url \"gopher://localhost/ with space\"|gopher://localhost/%20with%20space",
    "--accept-space --url \"https://localhost/?with space\"|https://localhost/?with+space",
    "https://daniel\@curl.se:22/ -s port= -s user=|https://curl.se/",
    "\"https://example.com?moo&search=hello\" --trim query=search|https://example.com/?moo",
    "\"https://example.com?search=hello&moo\" --trim query=search|https://example.com/?moo",
    "\"https://example.com?search=hello\" --trim query=search --append query=moo|https://example.com/?moo",
    "https://hello:443/foo|https://hello/foo",
    "ftp://hello:21/foo|ftp://hello/foo",
    "https://hello:443/foo -s scheme=ftp|ftp://hello:443/foo",
    "ftp://hello:443/foo -s scheme=https|https://hello/foo",
    "\"https://example.com?utm_source=tra%20cker&address%20=home&here=now&thisthen\" -g {query:utm_source}|tra cker",
    "\"https://example.com?utm_source=tra%20cker&address%20=home&here=now&thisthen\" -g {:query:utm_source}|tra%20cker",
    "\"https://example.com?utm_source=tra%20cker&address%20=home&here=now&thisthen\" -g {:query:utm_}|",
    "\"https://example.com?utm_source=tra%20cker&address%20=home&here=now&thisthen\" -g {:query:UTM_SOURCE}|",
    "\"https://example.com?utm_source=tracker&monkey=123\" --sort-query|https://example.com/?monkey=123&utm_source=tracker",
    "\"https://example.com?a=b&c=d&\" --sort-query|https://example.com/?a=b&c=d",
    "\"https://example.com?a=b&c=d&\" --sort-query --trim query=a |https://example.com/?c=d",
    "example.com:29 --set port=|http://example.com/",
    "--url HTTPS://example.com|https://example.com/",
    "--url https://EXAMPLE.com|https://EXAMPLE.com/",
    "--url https://example.com/FOO/BAR|https://example.com/FOO/BAR",
    "--url [2001:0db8:0000:0000:0000:ff00:0042:8329]|http://[2001:db8::ff00:42:8329]/",
    "\"https://example.com?utm=tra%20cker:address%20=home:here=now:thisthen\" --sort-query --query-separator \":\"|https://example.com/?address%20=home:here=now:thisthen:utm=tra%20cker",
    "\"foo?a=bCd=eCe=f\" --query-separator C --trim query=d|http://foo/?a=bCe=f",
    "localhost -g '{scheme} {host'|http {host",
    "localhost -g '[scheme] [host'|http [host",
    # two backslashes in the source end up one in the actual command line
    "localhost -g '\\{{scheme}\\['|{http[",
    "localhost -g '\\\\\['|\\[",
    "https://u:s\@foo?moo -g '[scheme][user][password][query]'|httpsusmoo",
    "\"hej?a=b&a=c&a=d&b=a\" -g '{query-all:a}'|b c d",
    "https://curl.se?name=mr%00smith --get {query:name}|mr.smith",
    "https://curl.se --iterate \"port=80 81 443\"|https://curl.se:80/\nhttps://curl.se:81/\nhttps://curl.se/",
    "https://curl.se --iterate 'port=81 443' --iterate scheme='sftp moo'|sftp://curl.se:81/\nmoo://curl.se:81/\nsftp://curl.se:443/\nmoo://curl.se:443/",
    "https://curl.se --iterate 'port=81 443'  --iterate scheme='sftp moo' --iterate port='2 1'|",
    "https://curl.se -s host=localhost --iterate port='22 23'|https://localhost:22/\nhttps://localhost:23/",

);

my %json_tests = (
    "\"https://example.com?utm=tra%20cker&address%20=home&here=now&thisthen\"" => [
        {
            "url" => "https://example.com/?utm=tra%20cker&address%20=home&here=now&thisthen",
            "scheme" => "https",
            "host" => "example.com",
            "port" => "443",
            "path" => "/",
            "query" => "utm=tra cker&address =home&here=now&thisthen",
             "params" => [
                 {
                     "key" => "utm",
                     "value" => "tra cker",
                 },
                 {
                     "key" => "address ",
                     "value" => "home",
                 },
                 {
                     "key" => "here",
                     "value" => "now"
                 },
                 {
                     "key" => "thisthen",
                    "value" => ""
                 }
             ]
        }
    ],
    "ftp://smith:secret\@example.com:33/path?search=me#where" => [
        {
            "url" => "ftp://smith:secret\@example.com:33/path?search=me#where",
            "scheme" =>  "ftp",
            "host" =>  "example.com",
            "port" =>  "33",
            "path" =>  "/path",
            "user" => "smith",
            "password" => "secret",
            "query" => "search=me",
            "fragment" => "where",
            "params" => [
                {
                    "key" => "search",
                    "value" => "me"
                }
            ]
        }
    ],
    "example.com" => [
        {
            "url" => "http://example.com/",
            "scheme" =>  "http",
            "host" =>  "example.com",
            "port" =>  "80",
            "path" =>  "/"
        }
    ],
    "example.com other.com" => [
        {
            "url" => "http://example.com/",
            "scheme" =>  "http",
            "host" =>  "example.com",
            "port" =>  "80",
            "path" =>  "/"
        },
        {
            "url" => "http://other.com/",
            "scheme" =>  "http",
            "host" =>  "other.com",
            "port" =>  "80",
            "path" =>  "/"
        }
    ]
);

plan tests => keys(@t) + keys(%json_tests);

for my $c (@t) {
    my ($i, $o) = split(/\|/, $c);
    # A future version should also check stderr
    my @out = ($^O eq 'MSWin32')?`.\\trurl.exe $i 2>nul`:`./trurl $i 2>/dev/null`;
    my $result = join("", @out);
    chomp $result;
    is( $result, $o, "./trurl $i" );
}

while (my($i, $o) = each %json_tests) {
    my @out = ($^O eq 'MSWin32')?`.\\trurl.exe --json $i 2>nul`:`./trurl --json $i 2>/dev/null`;
    my $result_json = join("", @out);
    my $result = decode_json($result_json);

    is_deeply($result, $o, "./trurl --json $i");
}

done_testing();
