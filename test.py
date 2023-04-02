#!/usr/bin/env python
from subprocess import check_output
from collections import deque
import platform
import os, sys

ptf = platform.uname()
if ptf.system == "Windows":
    bin = "urler.exe"
else:
    bin = "urler"

test_url = (
        "example.com|http://example.com/",
        "http://example.com|http://example.com/",
        "https://example.com|https://example.com/",
        "hp://example.com|hp://example.com/",
#        "|",
        "ftp.example.com|ftp://ftp.example.com/",
        "https://example.com/../moo|https://example.com/moo",
        "https://example.com/.././moo|https://example.com/moo",
        "https://example.com/test/../moo|https://example.com/moo",
        "localhost --append path=moo|http://localhost/moo",
        "--set host=moo --set scheme=http|http://moo/",
        "--url https://curl.se --set host=example.com|https://example.com/",
        "--set host=example.com --set scheme=ftp|ftp://example.com/",
        "--url https://curl.se/we/are.html --redirect here.html|https://curl.se/we/here.html",
        "--url https://curl.se/we/../are.html --set port=8080|https://curl.se:8080/are.html",
        "--url https://curl.se/we/are.html --get '{path}'|'/we/are.html'",
        "--url https://curl.se/we/are.html --get '{port}'|'443'",
        "--url https://curl.se/hello --append path=you|https://curl.se/hello/you",
        "--url https://curl.se?name=hello --append query=search=string|https://curl.se/?name=hello&search=string",
)

if __name__ == "__main__":

    for url in test_url:
        testin, testout = url.split("|")
        try:
            params = deque(testin.split(" "))
            params.appendleft(bin)
            result = (check_output(params).decode("utf-8")).rstrip(os.linesep)
            assert testout == result
        except AssertionError:
            print("FAIL\n")
            print(f"{testin} did not make {testout}\n")
            print(f"It showed {result}")
            sys.exit()

    print("success\n")
