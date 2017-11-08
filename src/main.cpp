#include "precompiled.h"

#include "util.h"
#include "stuff.h"
#include "stefanfw.h"

const int scale = 1;
int wsx, wsy;
Array2D<float> srcB;
Array2D<vec3> result;

float getB(Vec3f c)
{
  return sqrt(c.x*c.x+c.y*c.y+c.z*c.z);//(c.r+c.g+c.b);
}

inline Vec4f mul(Vec4f const& v, float f)
{
	__m128 f_loaded = _mm_load1_ps(&f);
	__m128& v_casted = (__m128&)v;
	__m128 result = _mm_mul_ps(f_loaded, v_casted);
	return (Vec4f&)result;
}

inline void addTo(Vec4f& dest, Vec4f const& v)
{
	//dest = (Vec4f&)_mm_add_ps((__m128&)dest, (__m128&)v);
	dest += v;
}

void aaPoint(Array2D<Vec4f>& dest, Vec2f const& pos, ColorA const& c) {
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
	Vec4f* addr = ((Vec4f*)dest.data)+y*dest.w+x;
	Vec4f const& c_ = (Vec4f const&)c;
	addTo(addr[0], mul(c_, UV));
	addTo(addr[1], mul(c_, uV));
	addTo(addr[dest.w], mul(c_, Uv));
	addTo(addr[dest.w + 1], mul(c_, uv));
}

float zero(Array2D<float> const&) { return 0.0f; }
Vec2f zero(Array2D<Vec2f> const&) { return Vec2f::zero(); }

template<class T>
T lerpFast(T const& a, T const& b, float coef)
{
	return a + coef * (b - a);
}

template<class Pixel, class TSurface>
Pixel fetchBilinear(TSurface const& src, Vec2f const& pos) {
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

bool mouseDown_[3];
bool keys[256];
float mouseX, mouseY;
bool keys2[256];
bool pause;

struct SApp : App {
		void keyDown(KeyEvent e)
	{
		keys[e.getChar()] = true;
		if(e.isControlDown()&&e.getCode()!=KeyEvent::KEY_LCTRL)
		{
			keys2[e.getChar()] = !keys2[e.getChar()];
			return;
		}
		if(keys['r'])
		{
		}
		if(keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	void keyUp(KeyEvent e)
	{
		keys[e.getChar()] = false;
	}
	
	void mouseDown(MouseEvent e)
	{
		mouseDown_[e.isLeft() ? 0 : e.isMiddle() ? 1 : 2] = true;
	}
	void mouseUp(MouseEvent e)
	{
		mouseDown_[e.isLeft() ? 0 : e.isMiddle() ? 1 : 2] = false;
	}
	void setup()
	{
		createConsole();

		Array2D<Vec3f> src = Surface8u(loadImage("test.png"), SurfaceConstraintsDefault(), false);
		setWindowSize(src.w, src.h);
		src = ::resize(src, src.Size() / ::scale, ci::FilterTriangle());

		vector<Array2D<float>> resultRGB;
		for(int chan = 0; chan < 3; chan++) {
			resultRGB.push_back(Array2D<float>(src.Size()));
		}
		
		for(int chan = 0; chan < 3; chan++) {
			srcB = Array2D<float>(src.Size());
			forxy(src) {
				srcB(p) = src(p)[chan];
			}
			auto gradients = ::get_gradients(srcB);

			auto func = [&](int yMin, int yMax) {
				for(int y = yMin; y < yMax; y++) {
					for(int x = 0; x < src.w; x++) {
						Aligned16Struct a16s;
						float& atXy_f = (float&)a16s;
						atXy_f = src(x, y)[chan];
						const float times = 50.0f;
						atXy_f /= times;
						Vec2f place(x, y);
						Vec2f gradientPersistent = Vec2f::zero();
						for(int i = 0; i < times; i++) {
							Vec2f& gradient = fetchBilinear<Vec2f>(gradients, place);
							//gradient = Vec2f(-gradient.y, gradient.x);
							gradientPersistent += gradient;
							place += gradientPersistent * 10;
							aaPoint(resultRGB[chan], place, atXy_f);
						}
					}
				}
			};
			boost::thread t(func, 0, src.h/2);
			func(src.h/2, src.h);
			t.join();
		}
		result = ::merge(resultRGB);
	}
	void draw()
	{
		gl::clear(Color(0, 0, 0));
		auto tex = gtex(result);
		gl::draw(tex, getWindowBounds());
	}
};

CINDER_APP(SApp, RendererGl)
