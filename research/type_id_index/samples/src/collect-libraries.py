#!/usr/bin/env python3
"""
Search directories recursively for shared libraries containing type_info objects.
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import List, Dict, Optional


def extract_typeinfo_symbols(library_path: str) -> List[str]:
    """
    Extract type_info symbols from a shared library using nm.

    Args:
        library_path: Path to the shared library file

    Returns:
        List of demangled type_info symbol names
    """
    try:
        # Use nm to extract and demangle dynamic symbols
        # -D: display dynamic symbols
        # -C: demangle C++ symbols
        # --defined-only: show only defined symbols
        nm_result = subprocess.run(
            ['nm', '-D', '-C', '--defined-only', library_path],
            capture_output=True,
            text=True,
            timeout=10
        )

        if nm_result.returncode != 0:
            return []

        typeinfo_symbols = []

        for line in nm_result.stdout.splitlines():
            # nm output format: address type symbol_name
            parts = line.split(None, 2)
            if len(parts) < 3:
                continue

            symbol_name = parts[2]

            # Check for typeinfo symbols
            # Demangled: "typeinfo for ClassName"
            # Mangled: starts with _ZTI
            if symbol_name.startswith('typeinfo for') or symbol_name.startswith('_ZTI'):
                typeinfo_symbols.append(symbol_name)

        return typeinfo_symbols

    except (subprocess.TimeoutExpired, FileNotFoundError, PermissionError):
        return []


def contains_type_info(library_path: str) -> Optional[int]:
    """
    Check if a library contains type_info symbols and count them.

    Args:
        library_path: Path to the shared library

    Returns:
        Number of type_info symbols found, or None if the file cannot be processed
    """
    symbols = extract_typeinfo_symbols(library_path)
    count = len(symbols)
    return count if count > 0 else None


def is_shared_library(path: Path) -> bool:
    """
    Check if a file is a shared library based on its extension.

    Args:
        path: Path to the file

    Returns:
        True if the file appears to be a shared library
    """
    name = path.name
    # Check for .so extension or .so.* pattern
    return path.suffix == '.so' or '.so.' in name


def scan_directory(directory: Path) -> List[Dict[str, any]]:
    """
    Scan a directory recursively for shared libraries with type_info objects.

    Args:
        directory: Path to the directory to scan

    Returns:
        List of library information dictionaries
    """
    libraries = []

    if not directory.exists() or not directory.is_dir():
        print(f"Warning: {directory} does not exist or is not a directory",
              file=sys.stderr)
        return libraries

    try:
        for root, dirs, files in os.walk(directory):
            for filename in files:
                filepath = Path(root) / filename

                if not filepath.is_file():
                    continue

                if is_shared_library(filepath):
                    count = contains_type_info(str(filepath))

                    if count is not None and count > 0:
                        libraries.append({
                            'path': str(filepath),
                            'typeinfo_count': count
                        })
                        print(f"Found: {filepath} ({count} type_info objects)")

    except PermissionError as e:
        print(f"Error scanning {directory}: {e}", file=sys.stderr)

    return libraries


def main():
    """Main entry point for the program."""
    parser = argparse.ArgumentParser(
        description='Search directories recursively for shared libraries '
                    'containing type_info objects.'
    )
    parser.add_argument(
        'paths',
        nargs='+',
        help='One or more directories to search recursively'
    )
    parser.add_argument(
        '-o', '--output',
        default='libraries.json',
        help='Output JSON file (default: libraries.json)'
    )

    args = parser.parse_args()

    # Check if required external command is available
    try:
        subprocess.run(['nm', '--version'],
                     capture_output=True,
                     timeout=1)
    except FileNotFoundError:
        print("Error: Required command 'nm' not found. "
              "Please install binutils.", file=sys.stderr)
        return 1

    all_libraries = []

    print("Scanning for shared libraries containing type_info objects...")

    for path_str in args.paths:
        path = Path(path_str)
        print(f"Scanning: {path}")
        libraries = scan_directory(path)
        all_libraries.extend(libraries)

    # Sort by typeinfo count (descending)
    all_libraries.sort(key=lambda x: x['typeinfo_count'], reverse=True)

    # Write to JSON file
    try:
        with open(args.output, 'w') as f:
            json.dump(all_libraries, f, indent=2)

        print(f"\nFound {len(all_libraries)} libraries with type_info objects")
        print(f"Results written to: {args.output}")

        return 0

    except IOError as e:
        print(f"Failed to write output file: {e}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
