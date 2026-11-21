"""Evaluation and live JS references over the Context's existing RPC channel."""

from ._public_values import decode_public, encode_public
from ._eval_options import source_text, source_offset

_TYPED = object()


class EvaluationControl:
    def eval(self, code, filename="<eval>", line=-1, col=-1, to_py=_TYPED,
             devtools=True, *, name=_TYPED):
        """Evaluate code; explicit to_py selects ruyijs's public value conversion.

        Without to_py the original typed snapshot protocol is preserved.
        to_py=False keeps DOM/class/function references; True labels top-level
        references. Ordinary containers become Python values in both modes.

        The code runs with host privilege: cross-origin frames are readable
        from it (contentDocument, their globals). Functions of the page it
        calls, and page scripts, keep the browser's cross-origin rules.
        """
        if name is not _TYPED:
            if filename != "<eval>":
                raise TypeError("use either filename or name")
            filename = name
        code, filename = source_text(code, "code"), source_text(filename, "name")
        line, col = source_offset(line, "line"), source_offset(col, "col")
        if "\0" in filename:
            raise ValueError("filename must not contain NUL")
        if to_py is not _TYPED and type(to_py) is not bool:
            raise TypeError("to_py must be a boolean")
        self._callbacks.guard()
        with self._lock:
            self._flush_host_releases()
            if to_py is _TYPED:
                return self._request("eval", code=code, filename=filename, line=line, col=col)
            packet = self._request("eval", code=code, filename=filename,
                                   result_mode="public", to_py=to_py, line=line, col=col)
            return decode_public(self, packet)

    @property
    def locals(self):
        """The original Window, including callable assignment and live members."""
        self._callbacks.guard()
        with self._lock:
            self._flush_host_releases()
            return decode_public(self, self._request("locals"))

    def _flush_host_releases(self):
        # Called under the Context lock. Finalizers only append; no JS runs
        # from a destructor or while a host callback is waiting to return.
        count = len(self._host_releases)
        if count:
            references = [self._host_releases.popleft() for _ in range(count)]
            self._request("host_value", action="release", references=references)

    def _host_value(self, reference, action, *, key=None, value=_TYPED):
        self._callbacks.guard()
        with self._lock:
            if self.closed:
                raise RuntimeError("ruyijs Context is closed")
            fields = {"reference": reference, "action": action}
            callbacks = {}
            if key is not None:
                fields["key"] = key
            if value is not _TYPED:
                fields["arguments" if action == "call" else "value"] = encode_public(self, value, callbacks)
            # 'value' keeps argument wrappers alive through release and call.
            self._flush_host_releases()
            # A setter may retain a callback before throwing. Once sent, keep
            # registration until close even when the operation reports error.
            self._callbacks.functions.update(callbacks)
            return decode_public(self, self._request("host_value", **fields))
