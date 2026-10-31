"""Cross-check native image decoders with Pillow-encoded format fixtures."""
import base64
import ctypes as c
import ctypes.util
import io
import os
import json
from pathlib import Path
import sys
import unittest
from PIL import Image

sys.path.insert(0, os.environ.get('RUYIJS_TEST_PACKAGE') or str(Path(__file__).resolve().parents[1] / 'python'))
from ruyijs import Context


class XY(c.Structure):
    _fields_ = [('x', c.c_double), ('y', c.c_double), ('Y', c.c_double)]


def linear_profile():
    lcms = c.CDLL(ctypes.util.find_library('lcms2'))
    lcms.cmsBuildGamma.argtypes = [c.c_void_p, c.c_double]
    lcms.cmsBuildGamma.restype = c.c_void_p
    lcms.cmsCreateRGBProfile.argtypes = [c.POINTER(XY), c.POINTER(XY), c.POINTER(c.c_void_p)]
    lcms.cmsCreateRGBProfile.restype = c.c_void_p
    lcms.cmsSaveProfileToMem.argtypes = [c.c_void_p, c.c_void_p, c.POINTER(c.c_uint32)]
    lcms.cmsCloseProfile.argtypes = [c.c_void_p]
    lcms.cmsFreeToneCurve.argtypes = [c.c_void_p]
    curves = (c.c_void_p*3)(*(lcms.cmsBuildGamma(None, 1.) for _ in range(3)))
    primaries = (XY*3)(XY(.64,.33,1), XY(.30,.60,1), XY(.15,.06,1))
    profile = lcms.cmsCreateRGBProfile(c.byref(XY(.3127,.3290,1)), primaries, curves)
    try:
        size = c.c_uint32()
        assert lcms.cmsSaveProfileToMem(profile, None, c.byref(size))
        data = c.create_string_buffer(size.value)
        assert lcms.cmsSaveProfileToMem(profile, data, c.byref(size))
        return data.raw[:size.value]
    finally:
        lcms.cmsCloseProfile(profile)
        for curve in curves: lcms.cmsFreeToneCurve(curve)


class Tests(unittest.TestCase):
    def decode(self, payload, mime, *, count=1, no_color=False):
        with Context(timeout=10) as ctx:
            ctx.load_page('https://images.test/', '<html><body></body></html>')
            ctx.set('encodedImage', base64.b64encode(payload).decode())
            ctx.set('mime', mime)
            ctx.eval('''globalThis.imageResult=null;(async()=>{
                const bytes=Uint8Array.from(atob(encodedImage),x=>x.charCodeAt(0));
                const decoder=new ImageDecoder({data:bytes,type:mime,colorSpaceConversion:'''
                + json.dumps('none' if no_color else 'default') + '''});
                await decoder.tracks.ready;
                const frames=[];
                for(let i=0;i<''' + str(count) + ''';i++){
                    const {image}=await decoder.decode({frameIndex:i});
                    const out=new Uint8Array(image.allocationSize({format:'RGBA'}));
                    await image.copyTo(out,{format:'RGBA'});
                    frames.push({width:image.codedWidth,height:image.codedHeight,
                        rgba:Array.from(out.slice(0,4)),duration:image.duration});
                    image.close();
                }
                imageResult={frames,count:decoder.tracks.selectedTrack.frameCount};decoder.close();
            })().catch(e=>imageResult={error:String(e),stack:e.stack});''')
            result = None
            for _ in range(150):
                ctx.advance(10)
                result = ctx.eval('imageResult')
                if result is not None: break
            self.assertIsNotNone(result)
            self.assertNotIn('error', result, result)
            return result

    def test_static_formats_and_alpha(self):
        for fmt, mime in [('PNG','image/png'),('JPEG','image/jpeg'),('GIF','image/gif'),
                          ('BMP','image/bmp'),('ICO','image/x-icon'),('WEBP','image/webp'),
                          ('AVIF','image/avif')]:
            with self.subTest(format=fmt):
                image = Image.new('RGBA',(16,16),(20,80,160,255))
                if fmt in ('JPEG','BMP'): image=image.convert('RGB')
                out=io.BytesIO()
                image.save(out,format=fmt,lossless=True,quality=100)
                result=self.decode(out.getvalue(),mime)['frames'][0]
                self.assertEqual([result['width'],result['height']],[16,16])
                tolerance=3 if fmt in ('JPEG','AVIF') else 0
                self.assertTrue(all(abs(a-b)<=tolerance for a,b in zip(result['rgba'],[20,80,160,255])), result)
        out=io.BytesIO();Image.new('RGBA',(2,2),(20,80,160,128)).save(out,format='PNG')
        rgba=self.decode(out.getvalue(),'image/png')['frames'][0]['rgba']
        self.assertEqual(rgba,[10,40,80,128]) # VideoFrame stores premultiplied data.

    def test_gif_webp_apng_animation(self):
        first=Image.new('RGB',(4,4),(255,0,0));second=Image.new('RGB',(4,4),(0,255,0))
        for fmt,mime in [('GIF','image/gif'),('WEBP','image/webp'),('PNG','image/png')]:
            with self.subTest(format=fmt):
                out=io.BytesIO();first.save(out,format=fmt,save_all=True,append_images=[second],duration=[40,80],loop=2,lossless=True)
                result=self.decode(out.getvalue(),mime,count=2)
                self.assertEqual(result['count'],2)
                self.assertEqual([f['rgba'] for f in result['frames']],[[255,0,0,255],[0,255,0,255]])
                self.assertEqual([f['duration'] for f in result['frames']],[40000,80000])

    def test_icc_conversion_and_opt_out(self):
        out=io.BytesIO();Image.new('RGB',(2,2),(128,128,128)).save(out,format='PNG',icc_profile=linear_profile())
        converted=self.decode(out.getvalue(),'image/png')['frames'][0]['rgba']
        raw=self.decode(out.getvalue(),'image/png',no_color=True)['frames'][0]['rgba']
        self.assertEqual(raw,[128,128,128,255])
        for value in converted[:3]: self.assertAlmostEqual(value,188,delta=1)


if __name__ == '__main__': unittest.main(verbosity=2)
