# Organizer-cli

Fast filesystem time base organizer, it move files based on time month year and extension

## Build

GCC/CLANG | C++20\
`make -j$(nproc)`
```txt
 ./organizor -h                                                                                                                 Usage: ./organizor [options]

Organize files in the source directory (top-level only).

Modes (choose one):
  (default)          <ext>/<file>
  --year             <ext>/<year>/<file>
  --month            <ext>/<year>/<month>/<file>   (implies --year)
  --day              <ext>/<year>/<month>/<day>/<file>
                     (implies --year and --month)
  --time             <year>/<month>/<file>   (no extension dir)
  --time --year      <year>/<file>
  --time --day       <year>/<month>/<day>/<file>

Options:
  -t                     Explicit type-only mode (same as default).
      --year             Include year directory.
      --month            Include year and month directories.
      --day              Include year, month and day directories.
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
