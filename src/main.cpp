#include "precompiled.h"

#include "ciextra.h"
#include "util.h"

ci::Surface32f src;
Array2D<float> srcB;
Array2D<Vec2f> gradients;
Array2D<Vec4f> result;

float getB(Color c)
{
  return sqrt(c.r*c.r+c.g*c.g+c.b*c.b);//(c.r+c.g+c.b);
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

	if(x<0||y<0 || x>=dest.getWidth()-1 || y>=dest.getHeight()-1)
		return;
	float uv = uRatio * vRatio;
	float Uv = vRatio - uv; // ((1-uRatio) * vRatio)
	float uV = uRatio - uv; // ((1-vRatio) * uRatio)
	float UV = 1 - uRatio - vRatio + uv; // (1-uRatio) * (1-vRatio)
	Vec4f* addr = ((Vec4f*)dest.getData())+y*dest.getWidth()+x;
	Vec4f const& c_ = (Vec4f const&)c;
	addTo(addr[0], mul(c_, UV));
	addTo(addr[1], mul(c_, uV));
	addTo(addr[dest.getWidth()], mul(c_, Uv));
	addTo(addr[dest.getWidth() + 1], mul(c_, uv));
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
	Pixel* addr = ((Pixel*)src.getData())+y*src.getWidth()+x;
	if(x<0||y<0 || x>=src.getWidth()-1 || y>=src.getHeight()-1)
		return zero(src);
	const int w = src.getWidth();
	return lerpFast(
		lerpFast(addr[0], addr[1], u_ratio),
		lerpFast(addr[w], addr[w+1], u_ratio),
		v_ratio);
}

__declspec(align(16)) struct Aligned16Struct {float a,b,c,d;};
// 11.78 sec
// made * into sse mul. 10.83 sec.
// made += into sse add. 13.64 sec.
struct SApp : AppBasic {
	void setup()
	{
		createConsole();

		/*Vec4f test(1, 2, 3, 4.5);
		auto test2 = mul(test, 2);
		cout << test2 << endl;
		Sleep(100*1000);*/
		ci::Timer timer;timer.start();
		src = Surface32f(loadImage("test.png"), SurfaceConstraintsDefault(), false);
		srcB = Array2D<float>(src.getWidth(), src.getHeight());
		gradients = Array2D<Vec2f>(src.getWidth(), src.getHeight());
		for(int y = 0; y < src.getHeight(); y++) {
			for(int x = 0; x < src.getWidth(); x++) {
				srcB(x, y) = getB(src.getPixel(Vec2i(x, y)));
			}
		}
		float maxDist = Vec3f::one().length();
		for(int y = 0; y < src.getHeight(); y++) {
			for(int x = 0; x < src.getWidth(); x++) {
				Vec2f place(x, y);

				Vec2f gradient = Vec2f::zero();
				Vec3f& here = (Vec3f&)src.getPixel(Vec2i(x, y));

				for (int i = -1; i <= 1; i++)
				{
					for (int j = -1; j <= 1; j++)
					{
						Vec3f& neighbour = (Vec3f&)src.getPixel(Vec2i(x + i, y + j));
						float colorDistance = neighbour.distance(here);
						float weight = 1.0f - colorDistance / maxDist;
						gradient += weight * Vec2f(i, j);
					}
				}
				/*
				float dx = -fetchBilinear<float>(srcB, place + Vec2f(-1, 0)) + fetchBilinear<float>(srcB, place + Vec2f(1, 0));
				float dy = -fetchBilinear<float>(srcB, place + Vec2f(0, -1)) + fetchBilinear<float>(srcB, place + Vec2f(0, 1));
					
				Vec2f gradient(dx, dy); //gradient.safeNormalize();
				//gradient = gradient.safeNormalized() * pow(gradient.length(), 2.0f);
				*/
				//gradient = Vec2f(-gradient.y, gradient.x); // perpendicular (tangent) rather than gradient
				gradients(x, y) = gradient;
			}
		}

		setWindowSize(src.getWidth(), src.getHeight());
		result = Array2D<Vec4f>(src.getWidth(), src.getHeight());
		
		auto func = [&](int yMin, int yMax) {
			for(int y = yMin; y < yMax; y++) {
				//cout << (int)(100 * y / (float)src.getHeight()) << "%" << endl;
				for(int x = 0; x < src.getWidth(); x++) {
					Aligned16Struct a16s;
					ColorA& atXy_f = (ColorA&)a16s;
					atXy_f = src.getPixel(Vec2i(x, y));
					const float times = 100.0f;
					atXy_f /= times;
					Vec2f place(x, y);
					Vec2f gradientPersistent = Vec2f::zero();
					for(int i = 0; i < times; i++) {
						Vec2f const& gradient = fetchBilinear<Vec2f>(gradients, place);
						gradientPersistent += gradient;
						//place -= gradientPersistent;
						gradientPersistent.rotate(.1);
						place += gradientPersistent * 5;
						aaPoint(result, place, atXy_f);
					}
				}
			}
		};
		boost::thread t(func, 0, src.getHeight()/2);
		func(src.getHeight()/2, src.getHeight());
		t.join();
		/*iter = result.getIter();
		while(iter.line()) while(iter.pixel()) {
			auto& pixel = (Color&)*iter.mPtr;
			pixel *= 3;
			pixel.r /= pixel.r+1;
			pixel.g /= pixel.g+1;
			pixel.b /= pixel.b+1;
		}*/
		cout << timer.getSeconds() << endl;
	}
	void mouseDown(MouseEvent e)
	{
	}
	void draw()
	{
		gl::clear(Color(0, 0, 0));
		gl::Texture tex(src.getWidth(), src.getHeight());
		tex.bind();
		glTexSubImage2D(GL_TEXTURE_2D,
			0, // level
			0, 0, // offset
			result.getWidth(), result.getHeight(),
			GL_RGBA,
			GL_FLOAT,
			result.data);
		gl::draw(tex, getWindowBounds());
	}
};

CINDER_APP_BASIC(SApp, RendererGl)
