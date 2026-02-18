#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Optional, Tuple


class Colors:
    GREEN = "\033[92m"
    RED = "\033[91m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    RESET = "\033[0m"
    BOLD = "\033[1m"


class TestResult:
    def __init__(
        self,
        name: str,
        passed: bool,
        output: str = "",
        error: str = "",
        duration: float = 0.0,
    ):
        self.name = name
        self.passed = passed
        self.output = output
        self.error = error
        self.duration = duration


class TestRunner:
    def __init__(self, itsuki_exe: str, verbose: bool = False, timeout: int = 10):
        self.itsuki_exe = itsuki_exe
        self.verbose = verbose
        self.timeout = timeout
        self.results: List[TestResult] = []

    def discover_tests(self, test_dir: Path) -> List[Path]:
        """Descubre todos los archivos .suki en el directorio de tests"""
        tests = list(test_dir.glob("**/*.suki"))
        extra_tests = [
            Path("test_compare.suki"),
        ]
        for extra in extra_tests:
            if extra.exists():
                tests.append(extra)
        return sorted(list(set(tests)))

    def run_test(self, test_file: Path) -> TestResult:
        """Ejecuta un test individual"""
        test_name = test_file.stem
        start_time = time.time()

        try:
            result = subprocess.run(
                [self.itsuki_exe, str(test_file)],
                capture_output=True,
                text=True,
                timeout=self.timeout,
                encoding="utf-8",
                errors="replace",
            )

            duration = time.time() - start_time
            output = result.stdout
            error = result.stderr

            expected_file = test_file.with_suffix(".expected")

            if expected_file.exists():
                with open(expected_file, "r", encoding="utf-8") as f:
                    expected_output = f.read().strip()

                actual_output = output.strip()
                passed = actual_output == expected_output

                if not passed and self.verbose:
                    error += f"\n--- Expected ---\n{expected_output}\n--- Got ---\n{actual_output}\n"
            else:
                passed = result.returncode == 0

            return TestResult(test_name, passed, output, error, duration)

        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            return TestResult(
                test_name, False, "", f"Test timeout ({self.timeout}s)", duration
            )
        except Exception as e:
            duration = time.time() - start_time
            return TestResult(test_name, False, "", str(e), duration)

    def run_all_tests(self, test_dir: Path, pattern: Optional[str] = None) -> None:
        """Ejecuta todos los tests"""
        tests = self.discover_tests(test_dir)

        if pattern:
            tests = [t for t in tests if pattern in t.name]

        if not tests:
            print(f"{Colors.YELLOW}No se encontraron tests{Colors.RESET}")
            return

        print(f"{Colors.BOLD}Ejecutando {len(tests)} tests...{Colors.RESET}\n")

        for test_file in tests:
            result = self.run_test(test_file)
            self.results.append(result)

            status = (
                f"{Colors.GREEN}PASS{Colors.RESET}"
                if result.passed
                else f"{Colors.RED}FAIL{Colors.RESET}"
            )
            print(f"{status} {result.name} ({result.duration:.3f}s)")

            if not result.passed and self.verbose:
                if result.error:
                    print(f"  Error: {result.error}")
                if result.output:
                    print(f"  Output: {result.output[:200]}")

    def print_summary(self) -> None:
        """Imprime resumen de resultados"""
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = total - passed
        total_time = sum(r.duration for r in self.results)

        print(f"\n{Colors.BOLD}{'=' * 60}{Colors.RESET}")
        print(f"{Colors.BOLD}RESUMEN{Colors.RESET}")
        print(f"{'=' * 60}")
        print(f"Total:   {total} tests")
        print(f"{Colors.GREEN}Pasados: {passed}{Colors.RESET}")
        print(f"{Colors.RED}Fallidos: {failed}{Colors.RESET}")
        print(f"Tiempo:  {total_time:.2f}s")
        print(f"{'=' * 60}")

        if failed > 0:
            print(f"\n{Colors.RED}Tests fallidos:{Colors.RESET}")
            for result in self.results:
                if not result.passed:
                    print(f"  - {result.name}")
                    if result.error:
                        print(f"    {result.error}")

    def get_exit_code(self) -> int:
        return 0 if all(r.passed for r in self.results) else 1


def main():
    parser = argparse.ArgumentParser(description="Itsuki Test Runner")
    parser.add_argument(
        "--exe", default="./itsuki.exe", help="Ruta al ejecutable de Itsuki"
    )
    parser.add_argument("--dir", default="tests/test_cases", help="Directorio de tests")
    parser.add_argument("--pattern", help="Patron para filtrar tests")
    parser.add_argument("--verbose", "-v", action="store_true", help="Modo verbose")
    parser.add_argument(
        "--timeout", type=int, default=10, help="Timeout por test (segundos)"
    )

    args = parser.parse_args()

    # Verificar que existe el ejecutable
    if not os.path.exists(args.exe):
        print(f"{Colors.RED}Error: no se encontro {args.exe}{Colors.RESET}")
        return 1

    # Verificar directorio de tests
    test_dir = Path(args.dir)
    if not test_dir.exists():
        print(f"{Colors.RED}Error: no se encontro {args.dir}{Colors.RESET}")
        return 1

    runner = TestRunner(args.exe, args.verbose, args.timeout)
    runner.run_all_tests(test_dir, args.pattern)
    runner.print_summary()

    return runner.get_exit_code()


sys.exit(main())
