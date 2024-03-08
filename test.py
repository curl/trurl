#!/usr/bin/env python3
##########################################################################
#                                  _   _ ____  _
#  Project                     ___| | | |  _ \| |
#                             / __| | | | |_) | |
#                            | (__| |_| |  _ <| |___
#                             \___|\___/|_| \_\_____|
#
# Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at https://curl.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# SPDX-License-Identifier: curl
#
##########################################################################

import sys
from os import getcwd, path
import json
import shlex
from subprocess import PIPE, run, Popen
from dataclasses import dataclass, asdict
from typing import Any, Optional, TextIO
import locale

PROGNAME = "trurl"
TESTFILE = "tests.json"
VALGRINDTEST = "valgrind"
VALGRINDARGS = ["--error-exitcode=1", "--leak-check=full", "-q"]

RED = "\033[91m"  # used to mark unsuccessful tests
NOCOLOR = "\033[0m"

EXIT_SUCCESS = 0
EXIT_ERROR = 1

@dataclass
class CommandOutput:
    stdout: Any
    returncode: int
    stderr: str


def testComponent(value, exp):
    if isinstance(exp, bool):
        result = value == 0 or value not in ("", [])
        if exp:
            return result
        else:
            return not result

    return value == exp

# checks if valgrind is installed
def check_valgrind():
    process = Popen(VALGRINDTEST + " --version",
                    shell=True, stdout=PIPE, stderr=PIPE, encoding="utf-8")
    output, error = process.communicate()
    if output.startswith(VALGRINDTEST) and not len(error):
        return True
    return False


def getcharmap():
    process = Popen("locale charmap", shell=True, stdout=PIPE, stderr=PIPE, encoding="utf-8");
    output, error = process.communicate()
    return output.strip()


class TestCase:
    def __init__(self, testIndex, runnerCmd, baseCmd, **testCase):
        self.testIndex = testIndex
        self.runnerCmd = runnerCmd
        self.baseCmd = baseCmd
        self.arguments = testCase["input"]["arguments"]
        self.expected = testCase["expected"]
        self.commandOutput: CommandOutput = None
        self.testPassed: bool = False

    def runCommand(self, cmdfilter: Optional[str], runWithValgrind: bool):
        # Skip test if none of the arguments contain the keyword
        if cmdfilter and all(cmdfilter not in arg for arg in self.arguments):
            return False

        cmd = [self.baseCmd]
        args = self.arguments
        if self.runnerCmd != "":
            cmd = [self.runnerCmd]
            args = [self.baseCmd] + self.arguments
        elif runWithValgrind:
            cmd = [VALGRINDTEST]
            args = VALGRINDARGS + [self.baseCmd] + self.arguments

        output = run(
            cmd + args,
            stdout=PIPE, stderr=PIPE,
            encoding="utf-8"
        )

        if isinstance(self.expected["stdout"], list):
            # if we don't expect string, parse to json
            try:
                stdout = json.loads(output.stdout)
            except json.decoder.JSONDecodeError:
                stdout = None
        else:
            stdout = output.stdout

        # assume stderr is always going to be string
        stderr = output.stderr

        # runners (e.g. wine) spill their own output into stderr,
        # ignore stderr tests when using a runner.
        if self.runnerCmd != "" and "stderr" in self.expected:
            stderr = self.expected["stderr"]

        self.commandOutput = CommandOutput(stdout, output.returncode, stderr)
        return True

    def test(self):
        # return true only if stdout, stderr and errorcode
        # are equal to the ones found in the testfile
        self.testPassed = all(
            testComponent(asdict(self.commandOutput)[k], exp)
            for k, exp in self.expected.items())
        return self.testPassed

    def _printVerbose(self, output: TextIO):
        self._printConcise(output)

        for component, exp in self.expected.items():
            value = asdict(self.commandOutput)[component]
            itemFail = self.commandOutput.returncode == 1 or \
                not testComponent(value, exp)

            print(f"--- {component} --- ", file=output)
            print("expected:", file=output)
            print("nothing" if exp is False else
                  "something" if exp is True else
                  f"{exp!r}",file=output)
            print("got:", file=output)

            header = RED if itemFail else ""
            footer = NOCOLOR if itemFail else ""
            print(f"{header}{value!r}{footer}", file=output)

        print()

    def _printConcise(self, output: TextIO):
        if self.testPassed:
            header = ""
            result = "passed"
            footer = ""
        else:
            header = RED
            result = "failed"
            footer = NOCOLOR
        text = f"{self.testIndex}: {result}\t{shlex.join(self.arguments)}"
        print(f"{header}{text}{footer}", file=output)


    def printDetail(self, verbose: bool = False, failed: bool = False):
        output: TextIO = sys.stderr if failed else sys.stdout
        if verbose:
            self._printVerbose(output)
        else:
            self._printConcise(output)


def main(argc, argv):
    ret = EXIT_SUCCESS
    baseDir = path.dirname(path.realpath(argv[0]))
    locale.setlocale(locale.LC_ALL, "")
    # python on windows does not always seem to find the
    # executable if it is in a different output directory than
    # the python script, even if it is in the current working
    # directory, using absolute paths to the executable and json
    # file makes it reliably find the executable
    baseCmd = path.join(getcwd(), PROGNAME)
    # the .exe on the end is necessary when using absolute paths
    if sys.platform == "win32" or sys.platform == "cygwin":
        baseCmd += ".exe"

    with open(path.join(baseDir, TESTFILE), "r", encoding="utf-8") as file:
        allTests = json.load(file)
        testIndexesToRun = []

    # if argv[1] exists and starts with int
    cmdfilter = ""
    testIndexesToRun = list(range(len(allTests)))
    runWithValgrind = False
    verboseDetail = False
    runnerCmd = ""

    if argc > 1:
        for arg in argv[1:]:
            if arg[0].isnumeric():
                # run only test cases separated by ","
                testIndexesToRun = []

                for caseIndex in arg.split(","):
                    testIndexesToRun.append(int(caseIndex))
            elif arg == "--with-valgrind":
                runWithValgrind = True
            elif arg == "--verbose":
                verboseDetail = True
            elif arg.startswith("--trurl="):
                baseCmd = arg[len("--trurl="):]
            elif arg.startswith("--runner="):
                runnerCmd = arg[len("--runner="):]
            else:
                cmdfilter = argv[1]

    if runWithValgrind and not check_valgrind():
        print(f'Error: {VALGRINDTEST} is not installed!', file=sys.stderr)
        return EXIT_ERROR

    # check if the trurl executable exists
    if path.isfile(baseCmd):
        # get the version info for the feature list
        args = ["--version"]
        if runnerCmd != "":
            cmd = [runnerCmd]
            args = [baseCmd] + args
        else:
            cmd = [baseCmd]
        output = run(
            cmd + args,
            stdout=PIPE, stderr=PIPE,
            encoding="utf-8"
        )
        features = output.stdout.split('\n')[1].split()[1:]

        numTestsFailed = 0
        numTestsPassed = 0
        numTestsSkipped = 0
        for testIndex in testIndexesToRun:
            # skip tests if required features are not met
            required = allTests[testIndex].get("required", None)
            if required and not set(required).issubset(set(features)):
                print(f"Missing feature, skipping test {testIndex + 1}.")
                numTestsSkipped += 1
                continue
            encoding = allTests[testIndex].get("encoding", None)
            if encoding and encoding != getcharmap():
                print(f"Invalid locale, skipping test {testIndex + 1}.")
                numTestsSkipped += 1
                continue;

            test = TestCase(testIndex + 1, runnerCmd, baseCmd, **allTests[testIndex])

            if test.runCommand(cmdfilter, runWithValgrind):
                if test.test():  # passed
                    test.printDetail(verbose=verboseDetail)
                    numTestsPassed += 1

                else:
                    test.printDetail(verbose=True, failed=True)
                    numTestsFailed += 1

        # finally print the results to terminal
        print("Finished:")
        result = ", ".join([
            f"Failed: {numTestsFailed}",
            f"Passed: {numTestsPassed}",
            f"Skipped: {numTestsSkipped}",
            f"Total: {len(testIndexesToRun)}"
        ])
        if (numTestsFailed == 0):
            print("Passed! - ", result)
        else:
            ret = f"Failed! - {result}"
    else:
        ret = f" error: File \"{baseCmd}\" not found!"
    return ret


if __name__ == "__main__":
    sys.exit(main(len(sys.argv), sys.argv))
