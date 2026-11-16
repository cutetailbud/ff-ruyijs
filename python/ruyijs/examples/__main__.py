"""List or run bundled examples without inventing a second runtime path.

``--shared`` runs several examples at once, each in its own thread, as
sessions of one shared Engine process instead of one process per Context.
"""
from pathlib import Path
import runpy
import sys
import threading
import time


EXAMPLES = {
    "akamai": ("akamai/akamai.py", "Akamai Bot Manager 3.x"),
    "datadome": ("DataDome/datadome.py", "DataDome 设备检测"),
    "creepjs": ("creepjs/creepjs.py", "CreepJS"),
    "truepeople": ("truepeople/truepeople.py", "TruePeopleSearch Cloudflare 托管质询"),
    "wildberries": ("wildberries/wildberries.py", "Wildberries 卖家页 wbaas 挑战与 risk_score"),
}


def _root():
    # Installed layout: examples sit next to this file (ruyijs/examples/<name>).
    # Source tree: this file is python/ruyijs/examples/__main__.py and the
    # examples live at <repo>/examples; the source tree has no copies here.
    local = Path(__file__).resolve().parent
    if (local / "akamai/akamai.py").is_file():
        return local
    return Path(__file__).resolve().parents[3] / "examples"


def _list():
    print("可用示例：")
    for name, (_, description) in EXAMPLES.items():
        print(f"  {name:<10} {description}")
    print("\n运行：python -m ruyijs.examples NAME [NAME 参数]")
    print("共享进程：python -m ruyijs.examples --shared NAME [NAME ...] [-- 每个示例的参数]")


def _script(name):
    item = EXAMPLES.get(name)
    if item is None:
        _list()
        print(f"\n未知示例：{name}", file=sys.stderr)
        return None
    root = _root().resolve()
    script = (root / item[0]).resolve()
    if not script.is_file() or not script.is_relative_to(root):
        print(f"示例文件不存在：{script}", file=sys.stderr)
        return None
    return script


class _Prefixed:
    """One console stream for several example threads: each line of a thread
    that has a name goes out whole, with that name in front."""

    def __init__(self, stream):
        self.stream = stream
        self._lock = threading.Lock()
        self._names, self._pending = {}, {}

    def name(self, label):
        self._names[threading.get_ident()] = label

    def write(self, text):
        ident = threading.get_ident()
        label = self._names.get(ident)
        if label is None:
            return self.stream.write(text)
        with self._lock:
            *lines, rest = (self._pending.get(ident, "") + text).split("\n")
            for line in lines:
                self.stream.write(f"[{label}] {line}\n")
            self._pending[ident] = rest
        return len(text)

    def flush(self):
        with self._lock:
            for ident, rest in self._pending.items():
                if rest:
                    self.stream.write(f"[{self._names[ident]}] {rest}\n")
            self._pending.clear()
        self.stream.flush()

    def __getattr__(self, name):
        return getattr(self.stream, name)


def _shared(names, argv):
    """Run `names` at once in one Engine; every Context() they make is a session."""
    from ruyijs import Engine, use_engine
    loaded = []
    for name in dict.fromkeys(names):
        script = _script(name)
        if script is None:
            return 2
        sys.path.insert(0, str(script.parent))
        module = runpy.run_path(str(script), run_name=f"ruyijs_example_{name}")
        if not callable(module.get("main")):
            print(f"示例没有 main()：{name}", file=sys.stderr)
            return 2
        loaded.append((name, module["main"]))
    results = {}
    out, err = _Prefixed(sys.stdout), _Prefixed(sys.stderr)
    sys.stdout, sys.stderr = out, err

    def run(name, main):
        out.name(name)
        err.name(name)
        begun = time.monotonic()
        try:
            code = main(list(argv))
        except SystemExit as error:
            code = error.code if isinstance(error.code, int) else 1
        except Exception as error:  # reported per example; the others go on
            print(f"{type(error).__name__}: {error}", file=sys.stderr)
            code = 1
        results[name] = (code or 0, time.monotonic() - begun)

    started = time.monotonic()
    try:
        with Engine(timeout=60) as engine, use_engine(engine):
            threads = [threading.Thread(target=run, args=item, name=item[0]) for item in loaded]
            for thread in threads:
                thread.start()
            for thread in threads:
                thread.join()
    finally:
        out.flush()
        err.flush()
        sys.stdout, sys.stderr = out.stream, err.stream
    print(f"\n共享进程运行 {len(loaded)} 个示例，用时 {time.monotonic() - started:.1f} 秒：")
    for name, (code, seconds) in results.items():
        print(f"  {name:<10} {'通过' if code == 0 else f'失败（{code}）'}  {seconds:.1f} 秒")
    return 0 if len(results) == len(loaded) and all(code == 0 for code, _ in results.values()) else 1


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ("-h", "--help", "list"):
        _list()
        return 0
    if argv[0] == "--shared":
        names, rest = argv[1:], []
        if "--" in names:
            index = names.index("--")
            names, rest = names[:index], names[index + 1:]
        if not names:
            _list()
            return 2
        return _shared(names, rest)
    name = argv.pop(0)
    script = _script(name)
    if script is None:
        return 2
    sys.path.insert(0, str(script.parent))
    sys.argv = [str(script), *argv]
    try:
        runpy.run_path(str(script), run_name="__main__")
    except SystemExit as error:
        return int(error.code) if isinstance(error.code, int) else 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
