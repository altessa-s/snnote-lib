# SNNote (C23)

Reading Supernote `.note` and `.mark` notebooks: pages, stored layer bitmaps, vector stroke logs, headings, keywords, links, text boxes,
recognition and document highlights. A standalone C23 library with no dependency beyond the C standard library and libm.

## Why it exists

The notebook holds both the tablet's rendered layers and the pen's vector records. A thumbnail needs the layers; a converter or search tool
needs the records and text. The library exposes both, with one owner for a parsed notebook and independent owners for decoded rasters.

## Building

A C23 compiler with typed enums, `constexpr`, `nullptr` and `[[nodiscard]]` is required: clang 19 or newer, or GCC 13 or newer.
Windows builds use clang with the Windows SDK. CMake 3.21 or newer is required for the build; vendored sources need no generated files.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target check
```

| Option | Default | Purpose |
|---|---|---|
| `BUILD_SHARED_LIBS` | `OFF` | Build a shared library |
| `SNNOTE_BUILD_TESTS` | On when top-level | Build the test suites |
| `SNNOTE_BUILD_TOOLS` | On when top-level | Build `sndump` |
| `SNNOTE_WERROR` | `OFF` | Treat compiler warnings as errors |
| `SNNOTE_LTO` | `ON` | Optimize shared Release builds across modules |
| `SNNOTE_BUILD_FUZZERS` | `OFF` | Build the libFuzzer harness with a supporting clang runtime |

Install headers, library, tool, CMake package and relocatable pkg-config metadata:

```sh
cmake --install build --prefix /usr/local
```

```cmake
set(CMAKE_C_STANDARD 23)
find_package(SNNote REQUIRED)
target_link_libraries(yours PRIVATE SNNote::snnote)
```

```sh
cc -std=c2x yours.c $(pkg-config --static --cflags --libs snnote)
```

## Reading

```c
#include <snnote/snnote.h>

sn_notebook* notebook = nullptr;
sn_error error;
if (sn_parse_notebook(data, size, &notebook, &error) == SN_OK) {
    for (size_t i = 0; i < notebook->page_count; i++) {
        const sn_page* page = &notebook->pages[i];
        /* page->strokes, page->layers, page->recognition */
    }
    sn_notebook_free(notebook);
}
```

The input is copied. All notebook arrays, strings and encoded bitmap bytes belong to one arena. Treat returned structures as read-only;
`sn_notebook_free` releases them together and accepts null. Strings carry byte lengths as well as NUL terminators, preserving embedded NULs.
Malformed UTF-8 text is repaired with U+FFFD. Recognition and highlight JSON require valid UTF-8.

| Status | Meaning |
|---|---|
| `SN_OK` | Complete operation, possibly with counted page or stroke recovery |
| `SN_ERR_INVALID_ARGUMENT` | Invalid pointer or dimension |
| `SN_ERR_OUT_OF_MEMORY` | Allocation failed; no partial result is returned |
| `SN_ERR_UNSUPPORTED_FORMAT` | Signature is not the supported notebook container |
| `SN_ERR_UNEXPECTED_EOF` | A required field lies beyond the input |
| `SN_ERR_MALFORMED` | Invalid structural data or an RLE stream that does not fill its sheet |
| `SN_ERR_IMAGE_ENCODING` | PNG or JPEG needs an external decoder |
| `SN_ERR_UNSUPPORTED_ENCODING` | Another bitmap protocol is not decoded |
| `SN_ERR_LIMIT` | Parse storage or work exceeds its budget, or raster dimensions exceed the raster ceiling |

Every fallible result leaves its output pointer unchanged on failure. The optional error contains a status, numeric detail and fixed-size message.
An unreadable page or stroke is skipped and counted. An unreadable annotation is dropped. Allocation failure always fails the entire parse. A declared block size whose storage alone exceeds the whole parse budget is malformed optional content and is recovered locally; accumulated budget exhaustion fails the parse. An ordinary unreadable outline preserves its readable centreline and is diagnosed.

Rectangle components that overflow to infinity are ignored like other invalid components; four valid components are required. Overflowing font sizes are absent. Highlight corner differences must remain finite or the highlight data is rejected. Original numeric text remains available in metadata and attributes.

Use `sn_bitmap_raster` for one RLE bitmap through `notebook->palette` into gray and alpha planes. Pages share the notebook palette. Free it with `sn_raster_free`. Page composition and PNG/JPEG decoding belong to a separate rendering library. The dump tool uses regression-only composition support.

`sn_bitmap_codes` preserves the RLE codes before palette mapping; it writes an owned `uint8_t*` and byte count through separate outputs, released with `sn_codes_free`.

## Writing

The library does not write notebooks or encode images. PNG and JPEG layers, templates, covers and annotation snapshots are handed over undecoded.

## Editing

The library does not edit notebook files. Returned models describe what was stored, including removed stroke records and hidden layers.

## Diagnostics

Install a process-wide sink with `sn_log_install`, and restore silence with `sn_log_silence`. The default is silent. Installation atomically
publishes the callback and context together; install at application startup. Callbacks run on the parsing thread and must synchronize shared state.
Messages contain fixed descriptions and counts, never notebook text, names, paths or URLs. Replaced callback bindings remain retained so concurrent
readers cannot use freed context bindings; the caller remains responsible for the lifetime of its context.
Reinstalling a retained sink/context pair allocates nothing. Memory grows only with distinct pairs: three pointer-sized fields (normally 24 bytes on a
64-bit host), plus allocator overhead, until process exit. Concurrent installs of the same new pair may retain duplicate bindings. Null installs and
`sn_log_silence` allocate nothing and reclaim nothing. A fresh context per install still grows memory without bound; install once at startup and
control filtering in the host sink if it must change frequently.
Callbacks must return normally (no longjmp, C++ exception or foreign-runtime panic).

## sndump

```sh
sndump notebook.note
sndump --json notebook.mark
```

The text dump is an ordered stream of field/value records. Array counts delimit their contents; strings use JSON escaping and doubles use
17 fractional digits in scientific notation. `--json` writes the same ordered records as an array of key/value pairs, allowing repeated keys.
Stored bytes are hexadecimal. Each page includes FNV-1a 64-bit digests of both composed raster planes, or the dedicated image-decoder status.
The tool walks and composes every page: N keys naming one large page incur N times that page’s traversal and composition work, even though the parser decodes the shared block once.
The canonical dump checksums under `tests/expected/` cover all 32 supplied notebooks.

## Design

Each parse owns its state; a thread-local work-meter binding is restored after each call, including nested calls. Metadata preserves order and repeated keys. Last page entries supersede earlier ones; resaved titles and keywords
replace an earlier entry only when the key and rectangle agree. Unknown device names are retained and sheet dimensions are measured from RLE.

Counts are checked against remaining bytes before reservation. Arithmetic is checked before multiplication. JSON nesting is limited to 64 containers,
LZ4 output to 255 times the compressed length (or 255 for an empty block), and raster allocation to 16,777,216 pixels. Stroke tail candidates require
fewer than 1,048,576 entries. Total live parse heap allocation (owner, arena chunks, headers and temporary scratch) is limited to 1 MiB plus 128 times the input byte count. Exhausting this budget by accumulation returns SN_ERR_LIMIT, releases all parse storage and leaves the output unchanged.

Parse work has a separate ceiling of **1,048,576 + 256 × input bytes** (`SN_PARSE_WORK_FLOOR` and `SN_PARSE_WORK_MULTIPLE` in `parser.h`). It charges examined bytes, allocation requests, scanner and tree traversal steps, sorting, comparisons and expansion. Exceeding it returns `SN_ERR_LIMIT` with a message naming the work budget, releases all parse storage and leaves the output unchanged. The multiple exceeds eight times the largest measured work/input ratio across the 32 fixtures and seven seeds; each is tested below one eighth of its ceiling. Host callbacks are outside the parse work budget. Standalone `sn_bitmap_codes` and `sn_bitmap_raster` use their pixel ceiling and single input without a parse work meter.

Protocol names up to **32 bytes** are interned as whole names. Longer names remain stored as unknown encodings and are never interned.

The legacy `SN_FILE_ASA_…` container is not read. Recognition archives, `EXTERNALLINKINFO` and `IDTABLE` are retained without interpreting their contents.
The document under a `.mark` file is not included in that file. Vector strokes are exposed but are not rendered by this library.

## Layout

| Path | Contents |
|---|---|
| `include/snnote/` | Public model, parsing, rasters, logging and umbrella header |
| `src/sn_internal.h` | Shared internal declarations |
| `src/sn_support.c`, `src/sn_bytes.c` | Arena, UTF-8, scalar reads and blocks |
| `src/sn_metadata.c`, `src/sn_json.c`, `src/sn_base64.c` | Metadata and structured text |
| `src/sn_lz4.c`, `src/sn_rle.c`, `src/sn_raster.c` | Decoders and raster operations |
| `src/sn_model.c`, `src/sn_strokes.c`, `src/sn_annotations.c`, `src/sn_parser.c` | Notebook assembly and interpretation |
| `src/sn_log.c` | Atomic diagnostics sink |
| `tools/sndump.c` | Deterministic dump tool |
| `tests/` | Corpus, recovery, mutation and decoder checks |

## Bindings

Consumers may vendor `src/*.c`, `src/sn_internal.h` and `include/snnote/*.h` directly. Compile as C23, add both include directories and link libm
where required. There are no generated headers, allocator hooks or dependency downloads.

## See also

[Format description](docs/FORMAT.md).

## Acknowledgements

The description of the unpublished `.note` and `.mark` formats is based on observations of Supernote files. The device samples were contributed by the project owner under the MIT licence. [Device sample descriptions](tests/fixtures/samples/README.md).

The Apache-2.0 test corpus comes from Brandon Philips’s supernote-typescript project. [Upstream project](https://github.com/philips/supernote-typescript) · [Fixture origins and licence](tests/fixtures/supernote-typescript/README.md).

## Trademark

Supernote is a trademark of Ratta Software. This project is not affiliated with, endorsed by or sponsored by Ratta Software.

## Licence

MIT — see [LICENSE](LICENSE). Copyright information is in [COPYRIGHT](COPYRIGHT).

Until 1.0, ABI stability is limited to a minor version; shared library compatibility uses major.minor for 0.x releases.

An outline declaration exceeding the entire parse budget rejects its stroke and increments the skipped-stroke count.
