"""Check that a configured profile reaches every realm of a Context (issue #51).

configure_fingerprint() before the first page sets the webgl, webgpu, media,
permissions and audio groups. Each realm of the Context reports them from its
own page script: the main document, an about:blank, a same-origin, a
cross-origin and a nested cross-origin iframe, a dedicated Worker started by
each of them, and the same after a top-level navigation (the next document and
its iframe). Every realm keeps its own constructors and objects; the values
must equal the profile. Offline: documents come from put_resource and the
proxy is a closed port. Exit status is non-zero on failure.
"""
import json
import sys

from ruyijs import Context

PROFILE = {
    "webgl": {"renderer": "R-MASKED-4070", "unmaskedRenderer": "R-UNMASKED-4070",
              "unmaskedVendor": "V-UNMASKED-NV"},
    "webgpu": {"enabled": True, "vendor": "nvidia", "architecture": "lovelace",
               "description": "GPU-DESC-4070"},
    "media": {"devices": ["audioinput|Mic|m1|g1", "videoinput|Cam|c1|g2", "videoinput|Cam2|c2|g3"]},
    "permissions": {"notifications": "granted"},
    "audio": {"sampleRate": 44100},
}
EXPECT = {"webgl": "R-MASKED-4070 / R-UNMASKED-4070 / V-UNMASKED-NV",
          "webgpu": "nvidia / lovelace / GPU-DESC-4070", "permissions": "granted",
          "media": "audioinput,videoinput,videoinput", "audio": 44100}

PROBE = r"""
async function __probe(scope) {
  const out = {};
  const canvas = scope === 'worker' ? new OffscreenCanvas(4, 4) : document.createElement('canvas');
  try {
    const gl = canvas.getContext('webgl');
    const ext = gl.getExtension('WEBGL_debug_renderer_info');
    out.webgl = [gl.getParameter(gl.RENDERER), gl.getParameter(ext.UNMASKED_RENDERER_WEBGL),
                 gl.getParameter(ext.UNMASKED_VENDOR_WEBGL)].join(' / ');
  } catch (e) { out.webgl = 'ERR ' + e; }
  try {
    const adapter = navigator.gpu && await navigator.gpu.requestAdapter();
    out.webgpu = adapter ? [adapter.info.vendor, adapter.info.architecture, adapter.info.description].join(' / ')
                         : String(adapter);
  } catch (e) { out.webgpu = 'ERR ' + e; }
  try { out.permissions = (await navigator.permissions.query({name: 'notifications'})).state; }
  catch (e) { out.permissions = 'ERR ' + e; }
  if (scope !== 'worker') {
    try { out.media = (await navigator.mediaDevices.enumerateDevices()).map(d => d.kind).join(','); }
    catch (e) { out.media = 'ERR ' + e; }
    try { const a = new AudioContext(); out.audio = a.sampleRate; a.close(); }
    catch (e) { out.audio = 'ERR ' + e; }
  }
  return out;
}
"""
# Each document probes itself, starts a Worker that probes, and posts both to top.
DOC = r"""<script>%s
const LABEL = %%s;
(async () => {
  top.postMessage({label: LABEL, value: await __probe('window')}, '*');
  const code = __probe.toString() + ';__probe("worker").then(v => postMessage(v), e => postMessage(String(e)));';
  const worker = new Worker(URL.createObjectURL(new Blob([code], {type: 'text/javascript'})));
  worker.onmessage = e => top.postMessage({label: LABEL + '/worker', value: e.data}, '*');
  worker.onerror = e => top.postMessage({label: LABEL + '/worker', value: 'worker error ' + e.message}, '*');
})();
</script>%%s""" % PROBE
COLLECT = "window.results = window.results || {}; addEventListener('message', e => { results[e.data.label] = e.data.value; });"


def doc(label, extra=""):
    return DOC % (json.dumps(label), extra)


def collect(ctx, labels):
    found = {}
    for _ in range(400):
        ctx.advance(25, max_tasks=1000)
        found = ctx.eval("results")
        if labels <= set(found):
            break
    return found


with Context(timeout=60, proxy="http://127.0.0.1:9") as ctx:
    ctx.configure_fingerprint(PROFILE)
    for url, body in [
            ("https://parent.invalid/same", doc("same-origin iframe")),
            ("https://child.invalid/cross", doc("cross-origin iframe",
                                                "<iframe src='https://grand.invalid/nested'></iframe>")),
            ("https://grand.invalid/nested", doc("nested cross-origin iframe")),
            ("https://parent.invalid/next", doc("next document",
                                                "<iframe src='https://child.invalid/next-cross'></iframe>")),
            ("https://child.invalid/next-cross", doc("next document's iframe"))]:
        ctx.put_resource(url, body, headers=[["Content-Type", "text/html"]])
    ctx.load_page("https://parent.invalid/", "<!doctype html><script>" + COLLECT + "</script>" + doc("main", """
<iframe src='/same'></iframe><iframe src='https://child.invalid/cross'></iframe>
<script>
const blank = document.createElement('iframe'); document.body.appendChild(blank);
blank.contentWindow.eval(__probe.toString());
blank.contentWindow.__probe('window').then(v => { results['about:blank iframe'] = v; });
</script>"""))
    first = ["main", "same-origin iframe", "cross-origin iframe", "nested cross-origin iframe"]
    results = collect(ctx, {*first, *(f"{label}/worker" for label in first), "about:blank iframe"})
    ctx.navigate("https://parent.invalid/next")
    ctx.eval(COLLECT)
    second = ["next document", "next document's iframe"]
    results.update(collect(ctx, {*second, *(f"{label}/worker" for label in second)}))

realms = [*first, "about:blank iframe", *second]
realms += [f"{label}/worker" for label in [*first, *second]]
failures = []
for realm in realms:
    value = results.get(realm)
    keys = ["webgl", "webgpu", "permissions"] + ([] if realm.endswith("/worker") else ["media", "audio"])
    bad = [f"{key}={value.get(key)!r}" for key in keys if value.get(key) != EXPECT[key]] \
        if isinstance(value, dict) else [f"no report ({value!r})"]
    print(f"{'ok  ' if not bad else 'FAIL'} {realm:34} {' '.join(bad)}", flush=True)
    if bad:
        failures.append(realm)
if failures:
    print("frame_profile_check FAILED:", ", ".join(failures))
    sys.exit(1)
print(f"frame_profile_check passed ({len(realms)} realms)")
