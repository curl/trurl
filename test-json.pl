#!/usr/bin/env perl

my %t = (
    "example.com" => '[
        {
            "url": "http://example.com/",
            "scheme":  "http",
            "host":  "example.com",
            "port":  "80",
            "path":  "/"
        }
    ]',
    "example.com other.com" => '[
        {
            "url": "http://example.com/",
            "scheme":  "http",
            "host":  "example.com",
            "port":  "80",
            "path":  "/"
        },
        {
            "url": "http://other.com/",
            "scheme":  "http",
            "host":  "other.com",
            "port":  "80",
            "path":  "/"
        }
    ]'
);

while (my($i, $o) = each %t) {
    $o =~ s/[ \t\r\n]+/ /g;

    # A future version should also check stderr
    my @out = `./trurl --json $i 2>/dev/null`;
    my $result = join("", @out);
    chomp $result;
    $result =~ s/[ \t\r\n]+/ /g;

    if($result ne $o) {
        print "FAIL\n";
        print "$i did not show \"$o\"\n";
        print "It showed \"$result\"\n";
        exit 1;
    }
}
print "success\n";
