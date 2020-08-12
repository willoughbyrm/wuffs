// After editing this file, run "go generate" in the ../data directory.

// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ---------------- Auxiliary - JSON

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__AUX__JSON)

#include <utility>

namespace wuffs_aux {

DecodeJsonResult::DecodeJsonResult(std::string&& error_message0,
                                   uint64_t cursor_position0)
    : error_message(std::move(error_message0)),
      cursor_position(cursor_position0) {}

std::string  //
DecodeJsonCallbacks::AppendByteString(std::string&& val) {
  return "wuffs_aux::DecodeJson: unexpected JSON byte string";
}

void  //
DecodeJsonCallbacks::Done(DecodeJsonResult& result,
                          sync_io::Input& input,
                          IOBuffer& buffer) {}

const char DecodeJson_BadJsonPointer[] =
    "wuffs_aux::DecodeJson: bad JSON Pointer";
const char DecodeJson_NoMatch[] = "wuffs_aux::DecodeJson: no match";

#define WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN                       \
  while (tok_buf.meta.ri >= tok_buf.meta.wi) {                           \
    if (tok_status.repr == nullptr) {                                    \
    } else if (tok_status.repr == wuffs_base__suspension__short_write) { \
      tok_buf.compact();                                                 \
    } else if (tok_status.repr == wuffs_base__suspension__short_read) {  \
      if (!io_error_message.empty()) {                                   \
        ret_error_message = std::move(io_error_message);                 \
        goto done;                                                       \
      } else if (cursor_index != io_buf->meta.ri) {                      \
        ret_error_message =                                              \
            "wuffs_aux::DecodeJson: internal error: bad cursor_index";   \
        goto done;                                                       \
      } else if (io_buf->meta.closed) {                                  \
        ret_error_message =                                              \
            "wuffs_aux::DecodeJson: internal error: io_buf is closed";   \
        goto done;                                                       \
      }                                                                  \
      io_buf->compact();                                                 \
      if (io_buf->meta.wi >= io_buf->data.len) {                         \
        ret_error_message =                                              \
            "wuffs_aux::DecodeJson: internal error: io_buf is full";     \
        goto done;                                                       \
      }                                                                  \
      cursor_index = io_buf->meta.ri;                                    \
      io_error_message = input.CopyIn(io_buf);                           \
    } else {                                                             \
      ret_error_message = tok_status.message();                          \
      goto done;                                                         \
    }                                                                    \
    if (WUFFS_JSON__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE != 0) {      \
      ret_error_message =                                                \
          "wuffs_aux::DecodeJson: internal error: bad WORKBUF_LEN";      \
      goto done;                                                         \
    }                                                                    \
    wuffs_base__slice_u8 work_buf = wuffs_base__empty_slice_u8();        \
    tok_status = dec->decode_tokens(&tok_buf, io_buf, work_buf);         \
  }                                                                      \
  wuffs_base__token token = tok_buf.data.ptr[tok_buf.meta.ri++];         \
  uint64_t token_len = token.length();                                   \
  if ((io_buf->meta.ri < cursor_index) ||                                \
      ((io_buf->meta.ri - cursor_index) < token_len)) {                  \
    ret_error_message =                                                  \
        "wuffs_aux::DecodeJson: internal error: bad token indexes";      \
    goto done;                                                           \
  }                                                                      \
  uint8_t* token_ptr = io_buf->data.ptr + cursor_index;                  \
  cursor_index += token_len

namespace {

// DecodeJson_SplitJsonPointer returns ("bar", 8) for ("/foo/bar/baz/qux", 5).
// It returns a 0 size_t when s has invalid JSON Pointer syntax.
std::pair<std::string, size_t>  //
DecodeJson_SplitJsonPointer(std::string& s, size_t i) {
  std::string fragment;
  while (i < s.size()) {
    char c = s[i];
    if (c == '/') {
      break;
    } else if (c != '~') {
      fragment.push_back(c);
      i++;
      continue;
    }
    i++;
    if (i >= s.size()) {
      return std::make_pair(std::string(), 0);
    }
    c = s[i];
    if (c == '0') {
      fragment.push_back('~');
      i++;
      continue;
    } else if (c == '1') {
      fragment.push_back('/');
      i++;
      continue;
    }
    return std::make_pair(std::string(), 0);
  }
  return std::make_pair(std::move(fragment), i);
}

std::string  //
DecodeJson_DecodeBackslashX(std::string& str,
                            uint8_t* token_ptr,
                            size_t token_len) {
  wuffs_base__slice_u8 encoded =
      wuffs_base__make_slice_u8(token_ptr, token_len);
  while (encoded.len > 0) {
    uint8_t decoded[64];
    constexpr bool src_closed = true;
    wuffs_base__transform__output o = wuffs_base__base_16__decode4(
        wuffs_base__make_slice_u8(&decoded[0], sizeof decoded), encoded,
        src_closed, WUFFS_BASE__BASE_16__DEFAULT_OPTIONS);
    if (o.status.is_error()) {
      return o.status.message();
    } else if ((o.num_dst > (sizeof decoded)) || (o.num_src > encoded.len)) {
      return "wuffs_aux::DecodeJson: internal error: inconsistent base16 "
             "decoding";
    }
    str.append(  // Convert from (uint8_t*).
        static_cast<const char*>(static_cast<void*>(&decoded[0])), o.num_dst);
    encoded.ptr += o.num_src;
    encoded.len -= o.num_src;
  }
  return "";
}

std::string  //
DecodeJson_WalkJsonPointerFragment(wuffs_base__token_buffer& tok_buf,
                                   wuffs_base__status& tok_status,
                                   wuffs_json__decoder::unique_ptr& dec,
                                   wuffs_base__io_buffer* io_buf,
                                   std::string& io_error_message,
                                   size_t& cursor_index,
                                   sync_io::Input& input,
                                   std::string& json_pointer_fragment) {
  std::string ret_error_message;
  while (true) {
    WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

    int64_t vbc = token.value_base_category();
    uint64_t vbd = token.value_base_detail();
    if (vbc == WUFFS_BASE__TOKEN__VBC__FILLER) {
      continue;
    } else if ((vbc != WUFFS_BASE__TOKEN__VBC__STRUCTURE) ||
               !(vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH)) {
      return DecodeJson_NoMatch;
    } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST) {
      goto do_list;
    }
    goto do_dict;
  }

do_dict:
  // Alternate between these two things:
  //  1. Decode the next dict key (a string). If it matches the fragment, we're
  //    done (success). If we've reached the dict's end (VBD__STRUCTURE__POP)
  //    so that there was no next dict key, we're done (failure).
  //  2. Otherwise, skip the next dict value.
  while (true) {
    for (std::string str; true;) {
      WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

      int64_t vbc = token.value_base_category();
      uint64_t vbd = token.value_base_detail();
      switch (vbc) {
        case WUFFS_BASE__TOKEN__VBC__FILLER:
          continue;

        case WUFFS_BASE__TOKEN__VBC__STRUCTURE:
          if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
            goto fail;
          }
          return DecodeJson_NoMatch;

        case WUFFS_BASE__TOKEN__VBC__STRING: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_0_DST_1_SRC_DROP) {
            // No-op.
          } else if (vbd &
                     WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_1_DST_1_SRC_COPY) {
            const char* ptr =  // Convert from (uint8_t*).
                static_cast<const char*>(static_cast<void*>(token_ptr));
            str.append(ptr, token_len);
          } else if (
              vbd &
              WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_1_DST_4_SRC_BACKSLASH_X) {
            ret_error_message =
                DecodeJson_DecodeBackslashX(str, token_ptr, token_len);
            if (!ret_error_message.empty()) {
              goto done;
            }
          } else {
            goto fail;
          }
          break;
        }

        case WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT: {
          uint8_t u[WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL];
          size_t n = wuffs_base__utf_8__encode(
              wuffs_base__make_slice_u8(
                  &u[0], WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL),
              static_cast<uint32_t>(vbd));
          const char* ptr =  // Convert from (uint8_t*).
              static_cast<const char*>(static_cast<void*>(&u[0]));
          str.append(ptr, n);
          break;
        }

        default:
          goto fail;
      }

      if (token.continued()) {
        continue;
      }
      if (str == json_pointer_fragment) {
        return "";
      }
      goto skip_the_next_dict_value;
    }

  skip_the_next_dict_value:
    for (uint32_t skip_depth = 0; true;) {
      WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

      int64_t vbc = token.value_base_category();
      uint64_t vbd = token.value_base_detail();
      if (token.continued() || (vbc == WUFFS_BASE__TOKEN__VBC__FILLER)) {
        continue;
      } else if (vbc == WUFFS_BASE__TOKEN__VBC__STRUCTURE) {
        if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
          skip_depth++;
          continue;
        }
        skip_depth--;
      }

      if (skip_depth == 0) {
        break;
      }
    }  // skip_the_next_dict_value
  }    // do_dict

do_list:
  do {
    wuffs_base__result_u64 result_u64 = wuffs_base__parse_number_u64(
        wuffs_base__make_slice_u8(
            static_cast<uint8_t*>(static_cast<void*>(
                const_cast<char*>(json_pointer_fragment.data()))),
            json_pointer_fragment.size()),
        WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
    if (!result_u64.status.is_ok()) {
      return DecodeJson_NoMatch;
    }
    uint64_t remaining = result_u64.value;
    if (remaining == 0) {
      goto check_that_a_value_follows;
    }
    for (uint32_t skip_depth = 0; true;) {
      WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

      int64_t vbc = token.value_base_category();
      uint64_t vbd = token.value_base_detail();
      if (token.continued() || (vbc == WUFFS_BASE__TOKEN__VBC__FILLER)) {
        continue;
      } else if (vbc == WUFFS_BASE__TOKEN__VBC__STRUCTURE) {
        if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
          skip_depth++;
          continue;
        }
        if (skip_depth == 0) {
          return DecodeJson_NoMatch;
        }
        skip_depth--;
      }

      if (skip_depth > 0) {
        continue;
      }
      remaining--;
      if (remaining == 0) {
        goto check_that_a_value_follows;
      }
    }
  } while (false);  // do_list

check_that_a_value_follows:
  while (true) {
    WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

    int64_t vbc = token.value_base_category();
    uint64_t vbd = token.value_base_detail();
    if (vbc == WUFFS_BASE__TOKEN__VBC__FILLER) {
      continue;
    }

    // Undo the last part of WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN, so that
    // we're only peeking at the next token.
    tok_buf.meta.ri--;
    cursor_index -= token_len;

    if ((vbc == WUFFS_BASE__TOKEN__VBC__STRUCTURE) &&
        (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__POP)) {
      return DecodeJson_NoMatch;
    }
    return "";
  }  // check_that_a_value_follows

fail:
  return "wuffs_aux::DecodeJson: internal error: unexpected token";
done:
  return ret_error_message;
}

}  // namespace

DecodeJsonResult  //
DecodeJson(DecodeJsonCallbacks&& callbacks,
           sync_io::Input&& input,
           wuffs_base__slice_u32 quirks,
           std::string json_pointer) {
  // Prepare the wuffs_base__io_buffer and the resultant error_message.
  wuffs_base__io_buffer* io_buf = input.BringsItsOwnIOBuffer();
  wuffs_base__io_buffer fallback_io_buf = wuffs_base__empty_io_buffer();
  std::unique_ptr<uint8_t[]> fallback_io_array(nullptr);
  if (!io_buf) {
    fallback_io_array = std::unique_ptr<uint8_t[]>(new uint8_t[4096]);
    fallback_io_buf = wuffs_base__ptr_u8__writer(fallback_io_array.get(), 4096);
    io_buf = &fallback_io_buf;
  }
  size_t cursor_index = 0;
  std::string ret_error_message;
  std::string io_error_message;

  do {
    // Prepare the low-level JSON decoder.
    wuffs_json__decoder::unique_ptr dec = wuffs_json__decoder::alloc();
    if (!dec) {
      ret_error_message = "wuffs_aux::DecodeJson: out of memory";
      goto done;
    }
    for (size_t i = 0; i < quirks.len; i++) {
      dec->set_quirk_enabled(quirks.ptr[i], true);
    }

    // Prepare the wuffs_base__tok_buffer.
    wuffs_base__token tok_array[256];
    wuffs_base__token_buffer tok_buf =
        wuffs_base__slice_token__writer(wuffs_base__make_slice_token(
            &tok_array[0], (sizeof(tok_array) / sizeof(tok_array[0]))));
    wuffs_base__status tok_status = wuffs_base__make_status(nullptr);

    // Prepare other state.
    uint32_t depth = 0;
    std::string str;

    // Walk the (optional) JSON Pointer.
    for (size_t i = 0; i < json_pointer.size();) {
      if (json_pointer[i] != '/') {
        ret_error_message = DecodeJson_BadJsonPointer;
        goto done;
      }
      std::pair<std::string, size_t> split =
          DecodeJson_SplitJsonPointer(json_pointer, i + 1);
      i = std::move(split.second);
      if (i == 0) {
        ret_error_message = DecodeJson_BadJsonPointer;
        goto done;
      }
      ret_error_message = DecodeJson_WalkJsonPointerFragment(
          tok_buf, tok_status, dec, io_buf, io_error_message, cursor_index,
          input, split.first);
      if (!ret_error_message.empty()) {
        goto done;
      }
    }

    // Loop, doing these two things:
    //  1. Get the next token.
    //  2. Process that token.
    while (true) {
      WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN;

      int64_t vbc = token.value_base_category();
      uint64_t vbd = token.value_base_detail();
      switch (vbc) {
        case WUFFS_BASE__TOKEN__VBC__FILLER:
          continue;

        case WUFFS_BASE__TOKEN__VBC__STRUCTURE: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
            ret_error_message = callbacks.Push(static_cast<uint32_t>(vbd));
            if (!ret_error_message.empty()) {
              goto done;
            }
            depth++;
            continue;
          }
          ret_error_message = callbacks.Pop(static_cast<uint32_t>(vbd));
          depth--;
          goto parsed_a_value;
        }

        case WUFFS_BASE__TOKEN__VBC__STRING: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_0_DST_1_SRC_DROP) {
            // No-op.
          } else if (vbd &
                     WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_1_DST_1_SRC_COPY) {
            const char* ptr =  // Convert from (uint8_t*).
                static_cast<const char*>(static_cast<void*>(token_ptr));
            str.append(ptr, token_len);
          } else if (
              vbd &
              WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_1_DST_4_SRC_BACKSLASH_X) {
            ret_error_message =
                DecodeJson_DecodeBackslashX(str, token_ptr, token_len);
            if (!ret_error_message.empty()) {
              goto done;
            }
          } else {
            goto fail;
          }
          if (token.continued()) {
            continue;
          }
          ret_error_message =
              (vbd & WUFFS_BASE__TOKEN__VBD__STRING__CHAIN_MUST_BE_UTF_8)
                  ? callbacks.AppendTextString(std::move(str))
                  : callbacks.AppendByteString(std::move(str));
          str.clear();
          goto parsed_a_value;
        }

        case WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT: {
          uint8_t u[WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL];
          size_t n = wuffs_base__utf_8__encode(
              wuffs_base__make_slice_u8(
                  &u[0], WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL),
              static_cast<uint32_t>(vbd));
          const char* ptr =  // Convert from (uint8_t*).
              static_cast<const char*>(static_cast<void*>(&u[0]));
          str.append(ptr, n);
          if (token.continued()) {
            continue;
          }
          goto fail;
        }

        case WUFFS_BASE__TOKEN__VBC__LITERAL: {
          ret_error_message =
              (vbd & WUFFS_BASE__TOKEN__VBD__LITERAL__NULL)
                  ? callbacks.AppendNull()
                  : callbacks.AppendBool(vbd &
                                         WUFFS_BASE__TOKEN__VBD__LITERAL__TRUE);
          goto parsed_a_value;
        }

        case WUFFS_BASE__TOKEN__VBC__NUMBER: {
          if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__FORMAT_TEXT) {
            if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_INTEGER_SIGNED) {
              wuffs_base__result_i64 r = wuffs_base__parse_number_i64(
                  wuffs_base__make_slice_u8(token_ptr, token_len),
                  WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
              if (r.status.is_ok()) {
                ret_error_message = callbacks.AppendI64(r.value);
                goto parsed_a_value;
              }
            }
            if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_FLOATING_POINT) {
              wuffs_base__result_f64 r = wuffs_base__parse_number_f64(
                  wuffs_base__make_slice_u8(token_ptr, token_len),
                  WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
              if (r.status.is_ok()) {
                ret_error_message = callbacks.AppendF64(r.value);
                goto parsed_a_value;
              }
            }
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_NEG_INF) {
            ret_error_message = callbacks.AppendF64(
                wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    0xFFF0000000000000ul));
            goto parsed_a_value;
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_POS_INF) {
            ret_error_message = callbacks.AppendF64(
                wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    0x7FF0000000000000ul));
            goto parsed_a_value;
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_NEG_NAN) {
            ret_error_message = callbacks.AppendF64(
                wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    0xFFFFFFFFFFFFFFFFul));
            goto parsed_a_value;
          } else if (vbd & WUFFS_BASE__TOKEN__VBD__NUMBER__CONTENT_POS_NAN) {
            ret_error_message = callbacks.AppendF64(
                wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                    0x7FFFFFFFFFFFFFFFul));
            goto parsed_a_value;
          }
          goto fail;
        }
      }

    fail:
      ret_error_message =
          "wuffs_aux::DecodeJson: internal error: unexpected token";
      goto done;

    parsed_a_value:
      if (!ret_error_message.empty() || (depth == 0)) {
        goto done;
      }
    }
  } while (false);

done:
  DecodeJsonResult result(
      std::move(ret_error_message),
      wuffs_base__u64__sat_add(io_buf->meta.pos, cursor_index));
  callbacks.Done(result, input, *io_buf);
  return result;
}

#undef WUFFS_AUX__DECODE_JSON__GET_THE_NEXT_TOKEN

}  // namespace wuffs_aux

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__AUX__JSON)
