// After editing this file, run "go generate" in this directory.

// This file contains C-like code. It is close enough to C so that clang-format
// can format it well. But it is not C code per se. It contains templates that
// the code generator expands via a custom mechanism.
//
// This file has a .h extension, not a .c extension, so that Go tools such as
// "go generate" don't try to build it as part of the Go package, via cgo.

template short_read(string qPKGPREFIXq, string qnameq) {
short_read_qnameq:
  if (a_qnameq.buf && a_qnameq.buf->closed && !a_qnameq.limit.ptr_to_len) {
    status = qPKGPREFIXqERROR_UNEXPECTED_EOF;
    goto exit;
  }
  status = qPKGPREFIXqSUSPENSION_SHORT_READ;
  goto suspend;
}
