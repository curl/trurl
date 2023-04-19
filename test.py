#!/usr/bin/python3

from typing import Any, Optional
import json
import subprocess
import sys
import shlex
from dataclasses import dataclass, asdict

TESTFILE = "./tests.json"
BASECMD = "./trurl"

RED = "\033[91m"  # used to mark unsuccessful tests
NOCOLOR = "\033[0m"


@dataclass
class CommandOutput:
    stdout: Any
    returncode: int
    stderr: str


class TestCase:
    def __init__(self, testIndex, **testCase):
        self.testIndex = testIndex
        self.arguments = testCase["input"]["arguments"]
        self.expected = testCase["expected"]
        self.commandOutput: CommandOutput = None
        self.testPassed: bool = False

    def runCommand(self, cmdfilter: Optional[str]):
        # Skip test if none of the arguments contain the keyword
        if cmdfilter and all(cmdfilter not in arg for arg in self.arguments):
            return False

        output = subprocess.run(
            [BASECMD] + self.arguments,
            capture_output=True,
            encoding="utf-8"
        )

        if type(self.expected["stdout"]) == str:
            stdout = output.stdout
        else:
            # if we dont expect string, parse to json
            try:
                stdout = json.loads(output.stdout)
            except json.decoder.JSONDecodeError:
                stdout = None

        # assume stderr is always going to be string
        stderr = output.stderr

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
        result = 'passed' if self.testPassed else 'failed'
        print(f"{self.testIndex}: {result}\t{shlex.join(self.arguments)}")


def main():
    with open(TESTFILE, "r") as file:
        allTests = json.load(file)
        testIndexesToRun = []

    # if argv[1] exists and starts with int
    cmdfilter = ""
    testIndexesToRun = list(range(len(allTests)))

    if len(sys.argv) > 1:
        if sys.argv[1][0].isnumeric():
            # run only test cases separated by ","
            testIndexesToRun = []

            for caseIndex in sys.argv[1].split(","):
                testIndexesToRun.append(int(caseIndex))

        else:
            cmdfilter = sys.argv[1]

    numTestsPassed = 0
    for testIndex in testIndexesToRun:
        test = TestCase(testIndex, **allTests[testIndex])

        if test.runCommand(cmdfilter):
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
