"""Compile and check public cases, integer boundaries, and debug output."""

import os
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "CSI2P-II-Mini-master"


def run(command, source=""):
    result = subprocess.run(
        [str(part) for part in command], input=source, text=True,
        encoding="utf-8", errors="replace", capture_output=True,
        check=True, timeout=60,
    )
    return result.stdout


def main():
    suffix = ".exe" if os.name == "nt" else ""
    with tempfile.TemporaryDirectory(prefix="mini-compiler-") as directory:
        work = Path(directory)
        compiler = work / ("mini1" + suffix)
        debug = work / ("mini1_debug" + suffix)
        simulator = work / ("ASMC" + suffix)
        oracle = work / ("oracle" + suffix)
        for source, executable in [("mini1.c", compiler), ("mini1_debug.c", debug)]:
            run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                 "-O2", ROOT / source, "-o", executable])
        run(["g++", "-std=c++11", "-O2", UPSTREAM / "AssemblyCompiler/ASMC.cpp",
             "-o", simulator])

        cases = []
        for number in range(1, 7):
            source = (UPSTREAM / f"testcase/test{number}.in").read_text()
            initial = (-12, 86, 38) if number == 4 else (2, 3, 5)
            cases.append((f"public{number}", source, initial, number in (2, 3, 6)))
        for label, source in [
            ("postfix", "x=y++ +5;\nz=x*2;"),
            ("wide_constant", "x=2147483648/2;"),
            ("negative_wide_constant", "x=-(2147483648/2);"),
            ("minimum_int", "x=-2147483648;"),
            ("invalid_target", "++(x++);"),
        ]:
            cases.append((label, source, (2, 3, 5), label == "invalid_target"))

        for label, source, initial, invalid in cases:
            assembly = run([compiler], source)
            assert assembly == run([debug], source), f"{label}: debug output differs"
            actual = run([simulator, *initial], assembly)
            if invalid:
                assert assembly == "Compile Error!\n", (label, assembly)
                assert actual.strip() == "CE instruction found.", (label, actual)
                print(f"PASS {label}: expected compile error")
                continue

            oracle_source = work / "oracle.c"
            x, y, z = initial
            oracle_source.write_text(
                '#include <stdio.h>\nint main(void) {\n'
                f'int x={x}, y={y}, z={z};\n{source}\n'
                'printf("%d %d %d\\n", x, y, z);\nreturn 0;\n}\n',
                encoding="utf-8",
            )
            run(["gcc", "-std=c11", "-O0", "-ftrapv", oracle_source, "-o", oracle])
            expected = tuple(map(int, run([oracle]).split()))
            match = re.fullmatch(
                r"x, y, z = (-?\d+), (-?\d+), (-?\d+)\nTotal cycle = (\d+)\n", actual
            )
            assert match, (label, actual)
            assert tuple(map(int, match.groups()[:3])) == expected, (label, actual, expected)
            print(f"PASS {label}: {expected}, {match[4]} cycles")
        print(f"All {len(cases)} cases passed; debug and release assembly matched.")


if __name__ == "__main__":
    main()
