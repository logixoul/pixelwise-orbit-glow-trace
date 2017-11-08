#include "precompiled.h"

#include "util.h"
#include "stuff.h"
#include "stefanfw.h"

#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

int wsx = 600, wsy = 400;
const int scale = 2;
Array2D<float> srcB;
Array2D<vec3> result;

#define MULTILINE(...) #__VA_ARGS__

float getB(vec3 c)
{
  return sqrt(c.x*c.x+c.y*c.y+c.z*c.z);//(c.r+c.g+c.b);
}

bool pause;

inline vec4 mul(vec4 const& v, float f)
{
	__m128 f_loaded = _mm_load1_ps(&f);
	__m128& v_casted = (__m128&)v;
	__m128 result = _mm_mul_ps(f_loaded, v_casted);
	return (vec4&)result;
}

inline void addTo(vec4& dest, vec4 const& v)
{
	//dest = (vec4&)_mm_add_ps((__m128&)dest, (__m128&)v);
	dest += v;
}

void aaPoint(Array2D<vec4>& dest, vec2 const& pos, ColorA const& c) {
	int x = (int)pos.x;
	int y = (int)pos.y;
	float uRatio = pos.x - x;
	float vRatio = pos.y - y;

	if(x<0||y<0 || x>=dest.w-1 || y>=dest.h-1)
		return;
	float uv = uRatio * vRatio;
	float Uv = vRatio - uv; // ((1-uRatio) * vRatio)
	float uV = uRatio - uv; // ((1-vRatio) * uRatio)
	float UV = 1 - uRatio - vRatio + uv; // (1-uRatio) * (1-vRatio)
	vec4* addr = ((vec4*)dest.data)+y*dest.w+x;
	vec4 const& c_ = (vec4 const&)c;
	addTo(addr[0], mul(c_, UV));
	addTo(addr[1], mul(c_, uV));
	addTo(addr[dest.w], mul(c_, Uv));
	addTo(addr[dest.w + 1], mul(c_, uv));
}

float zero(Array2D<float> const&) { return 0.0f; }
vec2 zero(Array2D<vec2> const&) { return vec2(); }

template<class T>
T lerpFast(T const& a, T const& b, float coef)
{
	return a + coef * (b - a);
}

template<class Pixel, class TSurface>
Pixel fetchBilinear(TSurface const& src, vec2 const& pos) {
	int x = (int)pos.x;
	int y = (int)pos.y;
	float u_ratio = pos.x - x;
	float v_ratio = pos.y - y;
	Pixel* addr = ((Pixel*)src.data)+y*src.w+x;
	if(x<0||y<0 || x>=src.w-1 || y>=src.h-1)
		return zero(src);
	const int w = src.w;
	return lerpFast(
		lerpFast(addr[0], addr[1], u_ratio),
		lerpFast(addr[w], addr[w+1], u_ratio),
		v_ratio);
}

__declspec(align(16)) struct Aligned16Struct {float a,b,c,d;};
// 11.78 sec
// made * into sse mul. 10.83 sec.
// made += into sse add. 13.64 sec.

struct SApp : App {
	void setup()
	{
		createConsole();
		enableDenormalFlushToZero();
		disableGLReadClamp();

		stefanfw::eventHandler.subscribeToEvents(*this);

		///////////
		
		Array2D<vec3> src = Surface8u(loadImage("test5.png"), SurfaceConstraintsDefault(), false);
		setWindowSize(src.w, src.h);
		src = ::resize(src, src.Size() / ::scale, ci::FilterTriangle());

		srcB = Array2D<float>(src.Size());
		forxy(src) {
			srcB(p) = getB(src(p));
		}
		auto gradients = ::get_gradients(srcB);

		result = Array2D<vec3>(src.Size());
		
		auto func = [&](int yMin, int yMax) {
			for(int y = yMin; y < yMax; y++) {
				for(int x = 0; x < src.w; x++) {
					Aligned16Struct a16s;
					vec3& atXy_f = (vec3&)a16s;
					atXy_f = src(x, y);
					const float times = 100.0f;
					atXy_f /= times;
					vec2 place(x, y);
					vec2 gradientPersistent;
					for(int i = 0; i < times; i++) {
						vec2& gradient = fetchBilinear<vec2>(gradients, place);
						//gradient = vec2(-gradient.y, gradient.x);
						gradientPersistent += gradient;
						place += gradientPersistent * 10.0f;
						aaPoint(result, place, atXy_f);
					}
				}
			}
		};
		std::thread t(func, 0, src.h/2);
		func(src.h/2, src.h);
		t.join();
	}
	void update()
	{
		stefanfw::beginFrame();
		stefanUpdate();
		stefanDraw();
		stefanfw::endFrame();
	}
	void keyDown(KeyEvent e)
	{
		if (keys['r'])
		{
		}
		if (keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	void stefanUpdate() {
	}
	void stefanDraw()
	{
		gl::clear(Color(0, 0, 0));
		auto tex = gtex(result);
		gl::draw(tex, getWindowBounds());
	}
};

CINDER_APP(SApp, RendererGl)