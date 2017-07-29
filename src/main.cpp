#include "precompiled.h"

#include "util.h"
#include "stuff.h"
#include <CL/cl.hpp>

const int scale = 2;
Array2D<float> srcB;
Array2D<Vec3f> result;

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

struct SApp : AppBasic {
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

	void testCL() {
				std::vector<cl::Platform> all_platforms;
		cl::Platform::get(&all_platforms);
		
		cl::Platform default_platform=all_platforms[0];
		std::cout << "Using platform: "<<default_platform.getInfo<CL_PLATFORM_NAME>()<<std::endl;

		std::vector<cl::Device> all_devices;
	    default_platform.getDevices(CL_DEVICE_TYPE_GPU, &all_devices);
		cl::Device default_device=all_devices[0];
		std::cout<< "Using device: "<<default_device.getInfo<CL_DEVICE_NAME>()<<std::endl;

		cl::Context context(default_device);

		cl::Program::Sources sources;

		
		// kernel calculates for each element C=A+B
		std::string kernel_code=
				"   void kernel simple_add(global const int* A, global const int* B, global int* C){       "
				"       C[get_global_id(0)]=A[get_global_id(0)]+B[get_global_id(0)];                 "
				"   }                                                                               ";
		sources.push_back(std::make_pair<const char*, size_t>(kernel_code.c_str(),kernel_code.length()));
 
		cl::Program program(context,sources);
		std::vector<cl::Device> deviceArg = boost::assign::list_of(default_device);
		if(program.build(deviceArg)!=CL_SUCCESS){
			std::cout<<" Error building: "<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device)<<"\n";
			exit(1);
		}
 
 
		// create buffers on the device
		cl::Buffer buffer_A(context,CL_MEM_READ_WRITE,sizeof(int)*10);
		cl::Buffer buffer_B(context,CL_MEM_READ_WRITE,sizeof(int)*10);
		cl::Buffer buffer_C(context,CL_MEM_READ_WRITE,sizeof(int)*10);
 
		int A[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
		int B[] = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
 
		//create queue to which we will push commands for the device.
		cl::CommandQueue queue(context,default_device);
 
		//write arrays A and B to the device
		queue.enqueueWriteBuffer(buffer_A,CL_FALSE,0,sizeof(int)*10,A);
		queue.enqueueWriteBuffer(buffer_B,CL_FALSE,0,sizeof(int)*10,B);
 
 
		//run the kernel
		cl::make_kernel<cl::Buffer&,cl::Buffer&,cl::Buffer&> simple_add(cl::Kernel(program,"simple_add"));
		cl::EnqueueArgs eargs(queue,cl::NullRange,cl::NDRange(10),cl::NullRange);
		simple_add(eargs, buffer_A,buffer_B,buffer_C);
 
		//alternative way to run the kernel
		/*cl::Kernel kernel_add=cl::Kernel(program,"simple_add");
		kernel_add.setArg(0,buffer_A);
		kernel_add.setArg(1,buffer_B);
		kernel_add.setArg(2,buffer_C);
		queue.enqueueNDRangeKernel(kernel_add,cl::NullRange,cl::NDRange(10),cl::NullRange);
		queue.finish();*/
 
		int C[10];
		//read result C from the device to array C
		queue.enqueueReadBuffer(buffer_C,CL_TRUE,0,sizeof(int)*10,C);
 
		std::cout<<" result: \n";
		for(int i=0;i<10;i++){
			std::cout<<C[i]<<" ";
		}
		std::cout << std::endl;
	}

	void setup()
	{
		createConsole();

		////////////

		testCL();

		///////////
		
		Array2D<Vec3f> src = Surface8u(loadImage("test5.png"), SurfaceConstraintsDefault(), false);
		setWindowSize(src.w, src.h);
		src = ::resize(src, src.Size() / ::scale, ci::FilterTriangle());

		srcB = Array2D<float>(src.Size());
		forxy(src) {
			srcB(p) = getB(src(p));
		}
		auto gradients = ::get_gradients(srcB);

		result = Array2D<Vec3f>(src.Size());
		
		auto func = [&](int yMin, int yMax) {
			for(int y = yMin; y < yMax; y++) {
				for(int x = 0; x < src.w; x++) {
					Aligned16Struct a16s;
					Vec3f& atXy_f = (Vec3f&)a16s;
					atXy_f = src(x, y);
					const float times = 100.0f;
					atXy_f /= times;
					Vec2f place(x, y);
					Vec2f gradientPersistent = Vec2f::zero();
					for(int i = 0; i < times; i++) {
						Vec2f& gradient = fetchBilinear<Vec2f>(gradients, place);
						//gradient = Vec2f(-gradient.y, gradient.x);
						gradientPersistent += gradient;
						place += gradientPersistent * 10;
						aaPoint(result, place, atXy_f);
					}
				}
			}
		};
		boost::thread t(func, 0, src.h/2);
		func(src.h/2, src.h);
		t.join();
	}
	void draw()
	{
		gl::clear(Color(0, 0, 0));
		auto tex = gtex(result);
		gl::draw(tex, getWindowBounds());
	}
};

CINDER_APP_BASIC(SApp, RendererGl)
