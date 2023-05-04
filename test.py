#!/usr/bin/env python3

import sys
from os import getcwd, path
import re
import json
import shlex
from subprocess import PIPE, run
from dataclasses import dataclass, asdict
from typing import Any, Optional

PROGNAME = "trurl"
TESTFILE = "tests.json"
VALGRINDTEST = "valgrind"
VALGRINDARGS = ["--error-exitcode=1", "--leak-check=full", "-q"]

RED = "\033[91m"  # used to mark unsuccessful tests
NOCOLOR = "\033[0m"


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


class TestCase:
    def __init__(self, testIndex, baseCmd, **testCase):
        self.testIndex = testIndex
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
        if runWithValgrind:
            cmd = [VALGRINDTEST]
            args = VALGRINDARGS + [self.baseCmd] + self.arguments

        output = run(
            cmd + args,
            stdout=PIPE, stderr=PIPE,
            encoding="utf-8"
        )

        if isinstance(self.expected["stdout"], list):
            # if we dont expect string, parse to json
            try:
                stdout = json.loads(output.stdout)
            except json.decoder.JSONDecodeError:
                stdout = None
        else:
            stdout = output.stdout

        # assume stderr is always going to be string
        stderr = output.stderr

        self.commandOutput = CommandOutput(stdout, output.returncode, stderr)
        return True

    def test(self):
        # return true only if stdout, stderr and errorcode is equal to the ones found in testfile
        self.testPassed = all(
            testComponent(asdict(self.commandOutput)[k], exp)
            for k, exp in self.expected.items())
        return self.testPassed

    def printVerbose(self):
        print(RED, end="")
        self.printConcise()
        print(NOCOLOR, end="")

        for component, exp in self.expected.items():
            value = asdict(self.commandOutput)[component]
            itemFail = self.commandOutput.returncode == 1 and \
                not testComponent(value, exp)

            print(f"--- {component} --- ")
            print("expected:")
            print("nothing" if exp is False else
                  "something" if exp is True else
                  f"{exp!r}")
            print("got:")
            if itemFail:
                print(RED, end="")
            print(f"{value!r}")
            if itemFail:
                print(NOCOLOR, end="")

        print()

    def printConcise(self):
        result = 'passed' if self.testPassed else 'failed'
        print(f"{self.testIndex}: {result}\t{shlex.join(self.arguments)}")


def main(argc, argv):
    ret = 0
    baseDir = path.dirname(path.realpath(argv[0]))
    # python on windows does not always seem to find the
    # executable if it is in a different output directory than
    # the python script, even if it is in the current working
    # directory, using absolute paths to the executable and json
    # file makes it reliably find the executable
    baseCmd = path.join(getcwd(), PROGNAME)
    # the .exe on the end is necessary when using absolute paths
    if sys.platform == "win32" or sys.platform == "cygwin":
        baseCmd += ".exe"
    # check if the trurl executable exists
    if path.isfile(baseCmd):
        # get the run-time and build-time libcurl versions
        output = run(
            [baseCmd, "--version"],
            stdout=PIPE, stderr=PIPE,
            encoding="utf-8"
        )
        features = output.stdout.split('\n')[1].split()[1:]

        with open(path.join(baseDir, TESTFILE), "r") as file:
            allTests = json.load(file)
            testIndexesToRun = []

        # if argv[1] exists and starts with int
        cmdfilter = ""
        testIndexesToRun = list(range(len(allTests)))
        runWithValgrind = False

        if argc > 1:
            for arg in argv[1:]:
                if arg[0].isnumeric():
                    # run only test cases separated by ","
                    testIndexesToRun = []

                    for caseIndex in arg.split(","):
                        testIndexesToRun.append(int(caseIndex))
                elif arg == "--with-valgrind":
                    runWithValgrind = True
                else:
                    cmdfilter = argv[1]

        numTestsFailed = 0
        numTestsPassed = 0
        numTestsSkipped = 0
        for testIndex in testIndexesToRun:
            # skip tests if required features are not met
            if "required" in allTests[testIndex]:
                required = allTests[testIndex]["required"]
                if not set(required).issubset(set(features)):
                    print(f"Missing feature, skipping test {testIndex + 1}.")
                    numTestsSkipped += 1
                    continue

            test = TestCase(testIndex + 1, baseCmd, **allTests[testIndex])

            if test.runCommand(cmdfilter, runWithValgrind):
                if test.test():  # passed
                    test.printConcise()
                    numTestsPassed += 1

                else:
                    test.printVerbose()
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
