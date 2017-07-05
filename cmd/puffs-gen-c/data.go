// Code generated by running "go generate". DO NOT EDIT.

// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

package main

const baseHeader = "" +
	"#ifndef PUFFS_BASE_HEADER_H\n#define PUFFS_BASE_HEADER_H\n\n// Use of this source code is governed by a BSD-style license that can be found\n// in the LICENSE file.\n\n#include <stdbool.h>\n#include <stdint.h>\n#include <string.h>\n\n// Puffs requires a word size of at least 32 bits because it assumes that\n// converting a u32 to usize will never overflow. For example, the size of a\n// decoded image is often represented, explicitly or implicitly in an image\n// file, as a u32, and it is convenient to compare that to a buffer size.\n//\n// Similarly, the word size is at most 64 bits because it assumes that\n// converting a usize to u64 will never overflow.\n#if __WORDSIZE < 32\n#error \"Puffs requires a word size of at least 32 bits\"\n#elif __WORDSIZE > 64\n#error \"Puffs requires a word size of at most 64 bits\"\n#endif\n\n// PUFFS_VERSION is the major.minor version number as a uint32. The major\n// number is the high 16 bits. The minor number is the low 16 bits.\n//\n// The intention is to bump the version number at least on every API " +
	"/ ABI\n// backwards incompatible change.\n//\n// For now, the API and ABI are simply unstable and can change at any time.\n//\n// TODO: don't hard code this in base-header.h.\n#define PUFFS_VERSION (0x00001)\n\n// puffs_base_buf1 is a 1-dimensional buffer (a pointer and length) plus\n// additional indexes into that buffer.\n//\n// A value with all fields NULL or zero is a valid, empty buffer.\ntypedef struct {\n  uint8_t* ptr;  // Pointer.\n  size_t len;    // Length.\n  size_t wi;     // Write index. Invariant: wi <= len.\n  size_t ri;     // Read  index. Invariant: ri <= wi.\n  bool closed;   // No further writes are expected.\n} puffs_base_buf1;\n\n// puffs_base_limit1 provides a limited view of a 1-dimensional byte stream:\n// its first N bytes. That N can be greater than a buffer's current read or\n// write capacity. N decreases naturally over time as bytes are read from or\n// written to the stream.\n//\n// A value with all fields NULL or zero is a valid, unlimited view.\ntypedef struct puffs_base_limit1 {\n  uint64_t* ptr_to_len" +
	";            // Pointer to N.\n  struct puffs_base_limit1* next;  // Linked list of limits.\n} puffs_base_limit1;\n\ntypedef struct {\n  puffs_base_buf1* buf;\n  puffs_base_limit1 limit;\n} puffs_base_reader1;\n\ntypedef struct {\n  puffs_base_buf1* buf;\n  puffs_base_limit1 limit;\n} puffs_base_writer1;\n\n#endif  // PUFFS_BASE_HEADER_H\n" +
	""

const baseImpl = "" +
	"#ifndef PUFFS_BASE_IMPL_H\n#define PUFFS_BASE_IMPL_H\n\n// Use of this source code is governed by a BSD-style license that can be found\n// in the LICENSE file.\n\n// Use switch cases for coroutine state, similar to the technique in\n// https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html\n//\n// We use a trivial macro instead of an explicit assignment and case statement\n// so that clang-format doesn't get confused by the unusual \"case\"s.\n#define PUFFS_COROUTINE_STATE(n) \\\n  coro_state = n;                \\\n  case n:\n\n#define PUFFS_LOW_BITS(x, n) ((x) & ((1 << (n)) - 1))\n\n// Clang also defines \"__GNUC__\".\n#if defined(__GNUC__)\n#define PUFFS_LIKELY(expr) (__builtin_expect(!!(expr), 1))\n#define PUFFS_UNLIKELY(expr) (__builtin_expect(!!(expr), 0))\n#else\n#define PUFFS_LIKELY(expr) (expr)\n#define PUFFS_UNLIKELY(expr) (expr)\n#endif\n\n#endif  // PUFFS_BASE_IMPL_H\n" +
	""
