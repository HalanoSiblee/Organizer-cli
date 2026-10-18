# Organizer-cli

<< ============================= <img src="logo.webp" width="128"/> ============================= >>

Fast filesystem time base organizer, it move files based on time month year and extension

## Build

GCC/CLANG | C++20\
`make -j$(nproc)`
```txt
 ./organizor -h                                                                                                                   10:19:31 PM
Usage: ./organizor [options]

Organize files in the source directory (top-level only).

Modes (choose one):
  (default)   <ext>/<file>
  --year      <ext>/<year>/<file>
  --month     <ext>/<year>/<month>/<file>   (implies --year)
  --time      <year>/<month>/<file>         (no extension dir)
  --time --year   <year>/<file>             (year only, no ext)

Options:
  -t                     Explicit type-only mode (same as default).
      --year             Include year directory.
      --month            Include year and month directories.
      --time             Date-only layout (drops extension dir).
  -e <ext>               Only process this extension (repeatable).
                         Dot optional, case-insensitive (e.g. -e jpg).
      --exclude <glob>   Skip filenames matching glob (repeatable).
      --no-skip-lists    Do not apply built-in skip extensions/names.
      --source <dir>     Where to read files from (default: CWD).
      --target <dir>     Where to build the organized tree (default: source).
      --output <dir>     Alias for --target.
  -n, --dry-run          Show what would happen; move nothing.
  -v, --verbose          Log every move.
  -q, --quiet            Suppress summary and confirmation prompt.
  -y, --yes              Skip the interactive confirmation.
      --force            Bypass the root/danger-path safety guard.
      --no-lowercase-ext Keep original extension case in dir name.
  -h, --help             Show this help.

```
