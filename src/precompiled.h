#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
#include <map>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <strstream>
#include <string>
#include <regex>
#include <complex>
#include <cinder/app/AppBasic.h>
#include <cinder/gl/GlslProg.h>
#include <cinder/gl/Texture.h>
#include <cinder/gl/gl.h>
#include <cinder/ImageIo.h>
#include <cinder/CinderMath.h>
#include <cinder/Vector.h>
#include <cinder/Rand.h>
#include "cinder/params/Params.h"
#include <cinder/Perlin.h>
#include <cinder/cairo/Cairo.h>
#include <boost/foreach.hpp>
#include <boost/assign.hpp>
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
//#include <boost/range/join.hpp>
#include <boost/foreach.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/range.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/numeric.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/any_range.hpp>
#include <boost/assign.hpp>
#define foreach BOOST_FOREACH
using namespace ci;
using namespace std;
using namespace ci::app;
using namespace boost::assign;
using boost::irange;