# Contributing to trurl
This document is intended to provide a framework for contributing to trurl. This document will go over requesting new features, fixing existing bugs and effectively
using the internal tooling to help PRs merge quickly.

## Opening an issue
trurl uses GitHubs issue tracking to track upcoming work. If you have a feature you want to add or find a bug simply open an issue in the
[issues tab](https://github.com/curl/trurl/issues). Briefly describe the feature you are requesting and why you think it may be valuable for trurl. If you are
reporting a bug be prepared for questions as we will want to reproduce it locally. In general providing the output of `trurl --version` along with the operating
system / Distro you are running is a good starting point.

## Writing a good PR
trurl is a relatively straightforward code base, so it is best to keep your PRs straightforward as well. Avoid trying to fix many bugs in one PR, and instead
use many smaller PRs as this avoids potential conflicts when merging. trurl is written in C and uses the [curl code style](https://curl.se/dev/code-style.html).
PRs that do not follow to code style will not be merged in.

trurl is in its early stages, so it's important to open a PR against a recent version of the source code, as a lot can change over a few days.
Preferably you would open a PR against the most recent commit in master.

If you are implementing a new feature, it must be submitted with tests and documentation. The process for writing tests is explained below in the tooling section. Documentation exists
in two locations, the man page ([trurl.1](https://github.com/curl/trurl/blob/master/trurl.1)) and the help prompt when running `trurl -h`. Most documentation changes
will go in the man page, but if you add a new command line argument then it must be documented in the help page.

It is also important to be prepared for feedback on your PR and adjust it promptly.


## Tooling
The trurl repository has a few small helper tools to make development easier.

**checksrc.pl** is used to ensure the code style is correct. It accepts C files as command line arguments, and returns nothing if the code style is valid. If the
code style is incorrect, checksrc.pl will provide the line the error is on and a brief description of what is wrong. You may run `make checksrc` to scan the entire
repository for style compliance.

**test.py** is used to run automated tests for trurl. It loads in tests from `test.json` (described below) and reports the number of tests passed. You may specify
the tests to run by passing a list of comma-separated numbers as command line arguments, such as `4,8,15,16,23,42` Note there is no space between the numbers. `test.py`
may also use valgrind to test for memory errors by passing `--with-valgrind` as a command line argument, it should be noted that this may take a while to run all the tests.
`test.py` will also skip tests that require a specific curl runtime or buildtime.

### Adding tests
Tests are located in [tests.json](https://github.com/curl/trurl/blob/master/tests.json). This file is an array of json objects when outline an input and what the expected
output should be. Below is a simple example of a single test:
```json
    {
        "input": {
            "arguments": [
                "https://example.com"
            ]
        },
        "expected": {
            "stdout": "https://example.com/\n",
            "stderr": "",
            "returncode": 0
        }
    }
  ```
  `"arguments"` is an array of the arguments to run in the test, so if you wanted to pass multiple arguments it would look something like:
  ```json
     {
        "input": {
            "arguments": [
                "https://curl.se:22/",
                "-s",
                "port=443",
                "--get",
                "{url}"
            ]
        },
        "expected": {
            "stdout": "https://curl.se/\n",
            "stderr": "",
            "returncode": 0
        }
    }
```
trurl may also return json. It you are adding a test that returns json to stdout, write the json directly instead of a string in the examples above. Below is an example
of what stdout should be if it is a json test, where `"input"` is what trurl accepts from the command line and `"expected"` is what trurl should return.
```json
"expected": {
    "stdout": [
        {
          "url": "https://curl.se/",
          "scheme": "https",
          "host": "curl.se",
          "port": "443",
          "raw_port": "",
          "path": "/",
          "query": "",
          "params": []
        }
    ],
    "returncode": 0,
    "stderr": ""
}
```

# Tips to make opening a PR easier
- Run `make checksrc` and `make test-memory` locally before opening a PR. These ran automatically when a PR is opened so you might as well make sure they pass before-hand.
- Update the man page and the help prompt accordingly. Documentation is annoying but if everyone writes a little it's not bad.
- Add tests to cover new features or the bug you fixed.
