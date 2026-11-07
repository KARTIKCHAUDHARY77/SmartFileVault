#!/usr/bin/env python3
"""Validate that vault_report.json remains parseable for legal special filenames."""

import argparse
import json
import pathlib
import subprocess
import tempfile


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=pathlib.Path)
    args = parser.parse_args()

    if not args.binary.is_file():
        raise SystemExit(f"Test executable not found: {args.binary}")

    with tempfile.TemporaryDirectory(prefix="sfv-vault-json-") as temporary:
        root = pathlib.Path(temporary)
        result = subprocess.run(
            [str(args.binary), str(root)],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.stdout:
            print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, end="")
        if result.returncode != 0:
            raise SystemExit(result.returncode)

        report_path = root / "vault_report.json"
        with report_path.open("r", encoding="utf-8") as report_file:
            report = json.load(report_file)

        assert report["archiveCount"] == 1
        assert len(report["archives"]) == 1
        assert report["archives"][0]["originalName"] == 'special\t"name.txt'
        assert isinstance(report["originalSize"], int)
        assert isinstance(report["compressedSize"], int)
        assert isinstance(report["spaceSaved"], int)
        print("PASS: report is valid JSON")
        print("PASS: tab and quote filename round-trips exactly")
        print("PASS: storage summary fields have numeric JSON types")

    print("ALL VAULT REPORT JSON TESTS PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
