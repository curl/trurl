#!/usr/bin/perl

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
    "--set host=moo --set scheme=http|http://moo/",
    "-s host=moo -s scheme=http|http://moo/",
    "--set host=moo --set scheme=https --set port=999|https://moo:999/",
    "--set host=moo --set scheme=ftps --set path=/hello|ftps://moo/hello",
    "--url https://curl.se --set host=example.com|https://example.com/",
    "--set host=example.com --set scheme=ftp|ftp://example.com/",
    "--url https://curl.se/we/are.html --redirect here.html|https://curl.se/we/here.html",
    "--url https://curl.se/we/../are.html --set port=8080|https://curl.se:8080/are.html",
    "--url https://curl.se/we/are.html --get '{path}'|/we/are.html",
    "--url https://curl.se/we/are.html --get '{port}'|443",
    "--url https://curl.se/we/are.html -g '{port}'|443",
    "--url https://curl.se/hello --append path=you|https://curl.se/hello/you",
    "--url \"https://curl.se?name=hello\" --append query=search=string|https://curl.se/?name=hello&search=string",
    "--url https://curl.se/hello --set user=:hej:|https://%3ahej%3a\@curl.se/hello",
    "localhost --append query=hello=foo|http://localhost/?hello=foo",
    "\"https://example.com?search=hello&utm_source=tracker\" --trim query=\"utm_*\"|https://example.com/?search=hello",
    "\"https://example.com?search=hello&utm_source=tracker&more=data\" --trim query=\"utm_*\"|https://example.com/?search=hello&more=data",
    "\"https://example.com?search=hello&more=data\" --trim query=\"utm_*\"|https://example.com/?search=hello&more=data",
    "\"https://example.com?utm_source=tracker\" --trim query=\"utm_*\"|https://example.com/",
    "\"https://example.com?search=hello&utm_source=tracker&more=data\" --trim query=\"utm_source\"|https://example.com/?search=hello&more=data",
    "\"https://example.com?search=hello&utm_source=tracker&more=data\" --trim query=\"utm_source\" --trim query=more --trim query=search|https://example.com/",
    "--accept-space --url \"gopher://localhost/ with space\"|gopher://localhost/%20with%20space",
    "--accept-space --url \"https://localhost/?with space\"|https://localhost/?with+space",
);

for my $c (@t) {
    my ($i, $o) = split(/\|/, $c);
    # A future version should also check stderr
    my @out = `./trurl $i 2>/dev/null`;
    my $result = join("", @out);
    chomp $result;
    if($result ne $o) {
        print "FAIL\n";
        print "$i did not show \"$o\"\n";
        print "It showed \"$result\"\n";
        exit 1;
    }
}
print "success\n";
