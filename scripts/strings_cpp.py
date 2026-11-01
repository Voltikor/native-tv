"""C++ message literal reader shared by translation checks."""
import re

CPP_ESCAPES = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0'}


def flush_bytes(val, pending):
    """Bytes written as \\xNN escapes, read as the UTF-8 they spell."""
    if pending:
        val.append(bytes(pending).decode('utf-8', 'replace'))
        del pending[:]


def cpp_literals(src, i):
    """Adjacent C++ string literals at src[i] ("a" "b" is one): (value, end) or None."""
    val, found, pending = [], False, bytearray()
    n = len(src)
    while True:
        while i < n and src[i].isspace():
            i += 1
        if src.startswith('//', i):
            i = src.find('\n', i)
            i = n if i < 0 else i
            continue
        if src.startswith('/*', i):
            i = src.find('*/', i + 2)
            i = n if i < 0 else i + 2
            continue
        m = re.compile(r'u8R"([^(\s]{0,16})\(|R"([^(\s]{0,16})\(').match(src, i)
        if m:                                              # raw string literal
            delim = m.group(1) if m.group(1) is not None else m.group(2)
            end = src.find(')' + delim + '"', m.end())
            if end < 0:
                return None
            val.append(src[m.end():end]); i = end + len(delim) + 2; found = True
            continue
        if i < n and src[i] == '"':
            j = i + 1
            while j < n and src[j] != '"':
                if src[j] == '\\':
                    nxt = src[j + 1]
                    if nxt == 'u':
                        val.append(chr(int(src[j + 2:j + 6], 16))); j += 6; continue
                    if nxt == 'x':                         # \xe2\x80\xa6 is UTF-8 bytes, here an ellipsis
                        k = j + 2
                        while k < n and src[k] in '0123456789abcdefABCDEF':
                            k += 1
                        pending.append(int(src[j + 2:k], 16) & 0xFF); j = k; continue
                    flush_bytes(val, pending)
                    val.append(CPP_ESCAPES.get(nxt, nxt)); j += 2; continue
                flush_bytes(val, pending)
                val.append(src[j]); j += 1
            flush_bytes(val, pending)
            i = j + 1; found = True
            continue
        return (''.join(val), i) if found else None

