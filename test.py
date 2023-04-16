#!/usr/bin/python3

from typing import Any
import json
import subprocess
import sys
import shlex
from dataclasses import dataclass, asdict

TESTFILE = "./tests.json"
if sys.platform == "win32" or sys.platform == "cygwin":
    BASECMD = "./trurl.exe"  # windows
else:
    BASECMD = "./trurl"  # linux

RED = "\033[91m"  # used to mark unsuccessful tests
NOCOLOR = "\033[0m"
TAB = "\x09"


@dataclass
class CommandOutput:
    stdout: Any
    returncode: int
    stderr: str


def stripDict(d):
    new = {}
    for key in d:
        if type(d[key]) == str:
            new[key] = d[key].strip()
        else:
            new[key] = d[key]

    return new


class TestCase:
    def __init__(self, testIndex, **testCase):
        self.testIndex = testIndex
        self.cmdline = testCase["cmdline"]
        self.expected = stripDict(testCase["expected"])
        self.commandOutput: CommandOutput = None
        self.testPassed: bool = False

    def runCommand(self, stdinkeyword: str):
        # returns false if the command does not contain keyword
        if self.cmdline.find(stdinkeyword) == -1:
            return False
        output = subprocess.run(
            shlex.split(f"{BASECMD} {self.cmdline}"),
            capture_output=True,
        )

        # testresult is bytes by default, change to string and remove line endings with strip if we expect a string
        if type(self.expected["stdout"]) == str:
            stdout = output.stdout.decode().strip()
        else:
            # if we dont expect string, parse to json
            stdout = json.loads(output.stdout)

        # assume stderr is always going to be string
        stderr = output.stderr.decode().strip()

        self.commandOutput = CommandOutput(stdout, output.returncode, stderr)
        return True

    def test(self):
        # return true only if stdout, stderr and errorcode is equal to the ones found in testfile

        tests = []
        for item in self.expected:
            passed = self.expected[item] == asdict(self.commandOutput)[item]
            tests.append(passed)

        self.testPassed = all(tests)
        return self.testPassed

    def printVerbose(self):
        print(RED, end="")
        self.printConcise()
        print(NOCOLOR, end="")

        for item in self.expected:
            itemFail = self.expected[item] != asdict(self.commandOutput)[item]

            print(f"--- {item} --- ")
            print("expected: ")
            print(f"'{self.expected[item]}'")
            print("got:")
            print(RED if itemFail else "", end="")
            print(f"'{asdict(self.commandOutput)[item]}'")
            print(NOCOLOR, end="")

        print()

    def printConcise(self):
        o = f"{self.testIndex}: {'passed' if self.testPassed else 'failed'}{TAB}'{self.cmdline}'"
        print(o)


def main():
    with open(TESTFILE, "r") as file:
        allTests = json.load(file)
        testIndexesToRun = []

    # if argv[1] exists and starts with int
    stdinfilter = ""
    testIndexesToRun = list(range(len(allTests)))

    if len(sys.argv) > 1:
        if sys.argv[1][0].isnumeric():
            # run only test cases separated by ","
            testIndexesToRun = []

            for caseIndex in sys.argv[1].split(","):
                testIndexesToRun.append(int(caseIndex))

        else:
            stdinfilter = sys.argv[1]

    numTestsPassed = 0
    for testIndex in testIndexesToRun:
        test = TestCase(testIndex, **allTests[testIndex])

        if test.runCommand(stdinfilter):
            if test.test():  # passed
                test.printConcise()
                numTestsPassed += 1

            else:
                test.printVerbose()

    # finally print the results to terminal

    run = print
    if (numTestsPassed != len(testIndexesToRun)):
        run = sys.exit

    print("finished:")
    run(f"{numTestsPassed}/{len(testIndexesToRun)} tests passed")


if __name__ == "__main__":
    main()
