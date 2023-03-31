#!/usr/bin/perl

my @t = (
    "example.com|http://example.com/",
    "http://example.com|http://example.com/",
    "https://example.com|https://example.com/",
    "hp://example.com|hp://example.com/",
    "|",
    "ftp.example.com|ftp://ftp.example.com/",
);

for my $c (@t) {
    my ($i, $o) = split(/\|/, $c);    
    my @out = `./urler $i`;
    my $result = join("", @out);
    chomp $result;
    if($result ne $o) {
        print "FAIL\n";
        print "$i did not make $o\n";
        print "$result\n";
        exit 1;
    }
}
print "success\n";
