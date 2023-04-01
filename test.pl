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
    "localhost --append-path moo|http://localhost/moo",
    "--set-host moo --set-scheme http|http://moo/",
);

for my $c (@t) {
    my ($i, $o) = split(/\|/, $c);
    # A future version should also check stderr
    my @out = `./urler $i 2>/dev/null`;
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
