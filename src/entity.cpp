#include "entity.hpp"

#include <fstream>
#include <string>
#include <algorithm>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glm/ext.hpp>

using namespace glm;
using namespace std;

Orbit::Orbit(
	const double ecc,
	const double sma,
	const double inc,
	const double lan,
	const double arg,
	const double pr,
	const double m0) :
	_ecc{ecc},
	_sma{sma},
	_inc{inc},
	_lan{lan},
	_arg{arg},
	_pr{pr},
	_m0{m0}
{

}

static double meanToEccentric(const double mean, const double ecc)
{
	// Newton to find eccentric anomaly (En)
	double En = (ecc<0.8)?mean:pi<float>(); // Starting value of En
	const int it = 20; // Number of iterations
	for (int i=0;i<it;++i)
		En -= (En - ecc*sin(En)-mean)/(1-ecc*cos(En));
	return En;
}

dvec3 Orbit::computePosition(
	const double epoch) const
{
	// Mean Anomaly compute
	const double meanMotion = 2*pi<float>()/_pr;
	const double meanAnomaly = fmod(epoch*meanMotion + _m0, 2*pi<float>());
	// Mean anomaly to Eccentric
	const double En = meanToEccentric(meanAnomaly, _ecc);
	// Eccentric anomaly to True anomaly
	const double trueAnomaly = 2*atan2(sqrt(1+_ecc)*sin(En/2), sqrt(1-_ecc)*cos(En/2));
	// Distance from parent body
	const double dist = _sma*((1-_ecc*_ecc)/(1+_ecc*cos(trueAnomaly)));
	// Plane changes
	const dvec3 posInPlane = dvec3(
		-sin(trueAnomaly)*dist,
		cos(trueAnomaly)*dist,
		0.0);
	const dquat q =
		  rotate(dquat(), _lan, dvec3(0,0,1))
		* rotate(dquat(), _inc, dvec3(0,1,0))
		* rotate(dquat(), _arg, dvec3(0,0,1));
	return q*posInPlane;
}

Atmo::Atmo(
	const vec4 K,
	const float density,
	const float maxHeight,
	const float scaleHeight) :
	_K{K},
	_density{density},
	_maxHeight{maxHeight},
	_scaleHeight{scaleHeight}
{

}

static float scatDensity(const float p, const float scaleHeight)
{
	return exp(-glm::max(0.f, p)/scaleHeight);
}

static float scatDensity(const vec2 p, const float radius, const float scaleHeight)
{
	return scatDensity(length(p) - radius, scaleHeight);
}

static float scatOptic(const vec2 a, const vec2 b, 
	const float radius, const float scaleHeight, const float maxHeight, const int samples)
{
	const vec2 step = (b-a)/(float)samples;
	vec2 v = a+step*0.5f;

	float sum = 0.f;
	for (int i=0;i<samples;++i)
	{
		sum += scatDensity(v, radius, scaleHeight);
		v += step;
	}
	return sum * length(step) / maxHeight;
}

static vec2 intersectsSphere(
	const vec2 ori, 
	const vec2 dir, 
	const float radius)
{
	const float b = dot(ori,dir);
	const float c = dot(ori,ori)-radius*radius;
	const float d = b*b-c;
	if (d < 0) return vec2(
		+numeric_limits<float>::infinity(), 
		-numeric_limits<float>::infinity());
	const float e = sqrt(d);
	return vec2(-b-e,-b+e);
}

vector<float> Atmo::generateLookupTable(
	const size_t size,
	const float radius) const
{
	/* 2 channel lookup table :
	 * y-axis for altitude (0.0 for sea level, 1.0 for maxHeight)
	 * x-axis for cosine of angle of ray
	 * First channel for air density
	 * Second channel for out scattering factor
	 */
	vector<float> table(size*size*2);

	size_t index = 0;
	for (size_t i=0;i<size;++i)
	{
		const float altitude = (float)i/(float)size * _maxHeight;
		const float density = exp(-altitude/_scaleHeight);
		for (size_t j=0;j<size;++j)
		{
			const float angle = acos(2*(float)j/(float)(size-1)-1);
			const vec2 rayDir = vec2(sin(angle), cos(angle));
			const vec2 rayOri = vec2(0, radius + altitude);
			const float t = intersectsSphere(rayOri, rayDir, radius+_maxHeight).y;
			const vec2 u = rayOri + rayDir*t;
			const float depth = scatOptic(rayOri, u, radius, _scaleHeight, _maxHeight, 50);
			table[index+0] = density;
			table[index+1] = depth;
			index += 2;
		}
	}
	return table;
}

vec4 Atmo::getScatteringConstant() const
{
	return _K;
}

float Atmo::getDensity() const
{
	return _density;
}

float Atmo::getMaxHeight() const
{
	return _maxHeight;
}

float Atmo::getScaleHeight() const
{
	return _scaleHeight;
}

Ring::Ring(
	const float innerDistance,
	const float outerDistance,
	const vec3 normal,
	const string &backscatFilename,
	const string &forwardscatFilename,
	const string &unlitFilename,
	const string &transparencyFilename,
	const string &colorFilename) :
	_innerDistance{innerDistance},
	_outerDistance{outerDistance},
	_normal{normalize(normal)},
	_backscatFilename{backscatFilename},
	_forwardscatFilename{forwardscatFilename},
	_unlitFilename{unlitFilename},
	_transparencyFilename{transparencyFilename},
	_colorFilename{colorFilename}
{

}

vector<float> Ring::loadFile(
	const string &filename) const
{
	ifstream in(filename);

	if (!in)
	{
		throw runtime_error("Can't open ring file " + filename);
	}

	// Clear values
	vector<float> pixelData;
	in.seekg(0, ios::beg);

	string number = "";

	while (!in.eof())
	{
		char c;
		in.read(&c, 1);
		if (c == ' ' || c == '\t' || c == '\n')
		{
			if (number.size() > 0)
			{
				pixelData.push_back(stof(number.c_str()));
				number = "";
			}
		}
		else
		{
			number += c;
		}
	}
	return pixelData;
}

float Ring::getInnerDistance() const
{
	return _innerDistance;
}

float Ring::getOuterDistance() const
{
	return _outerDistance;
}

vec3 Ring::getNormal() const
{
	return _normal;
}

string Ring::getBackscatFilename() const
{
	return _backscatFilename;
}

string Ring::getForwardscatFilename() const
{
	return _forwardscatFilename;
}

string Ring::getUnlitFilename() const
{
	return _unlitFilename;
}

string Ring::getTransparencyFilename() const
{
	return _transparencyFilename;
}

string Ring::getColorFilename() const
{
	return _colorFilename;
}

Model::Model(
	const float radius,
	const double GM,
	const vec3 rotAxis,
	const float rotPeriod,
	const vec3 meanColor,
	const string &diffuseFilename) :
	_rotAxis{normalize(rotAxis)},
	_rotPeriod{rotPeriod},
	_meanColor{meanColor},
	_radius{radius},
	_GM{GM},
	_diffuseFilename{diffuseFilename}
{

}

vec3 Model::getRotationAxis() const
{
	return _rotAxis;
}

float Model::getRotationPeriod() const
{
	return _rotPeriod;
}

vec3 Model::getMeanColor() const
{
	return _meanColor;
}

float Model::getRadius() const
{
	return _radius;
}

double Model::getGM() const
{
	return _GM;
}

string Model::getDiffuseFilename() const
{
	return _diffuseFilename;
}

Star::Star(const float brightness,
	const float flareFadeInStart, const float flareFadeInEnd,
	const float flareAttenuation, const float flareMinSize,
	const float flareMaxSize) : 
	_brightness{brightness},
	_flareFadeInStart{flareFadeInStart},
	_flareFadeInEnd{flareFadeInEnd},
	_flareAttenuation{flareAttenuation},
	_flareMinSize{flareMinSize},
	_flareMaxSize{flareMaxSize}
{

}

float Star::getBrightness() const
{
	return _brightness;
}

float Star::getFlareFadeInStart() const
{
	return _flareFadeInStart;
}

float Star::getFlareFadeInEnd() const
{
	return _flareFadeInEnd;
}

float Star::getFlareAttenuation() const
{
	return _flareAttenuation;
}

float Star::getFlareMinSize() const
{
	return _flareMinSize;
}

float Star::getFlareMaxSize() const
{
	return _flareMaxSize;
}

Clouds::Clouds(const string &filename, const float period) :
	_filename{filename},
	_period{period}
{

}

string Clouds::getFilename() const
{
	return _filename;
}

float Clouds::getPeriod() const
{
	return _period;
}

Night::Night(const string &filename,
	const float intensity) :
	_filename{filename},
	_intensity{intensity}
{

}

string Night::getFilename() const
{
	return _filename;
}

float Night::getIntensity() const
{
	return _intensity;
}

Specular::Specular(const string &filename,
	const Mask mask0, const Mask mask1) :
	_filename{filename},
	_mask0{mask0},
	_mask1{mask1}
{

}

Specular::Mask Specular::getMask0() const
{
	return _mask0;
}

Specular::Mask Specular::getMask1() const
{
	return _mask1;
}

string Specular::getFilename() const
{
	return _filename;
}

void EntityParam::setName(const string &name)
{
	_name = name;
}

void EntityParam::setDisplayName(const string &name)
{
	_displayName = name;
}

void EntityParam::setParentName(const string &name)
{
	_parentName = name;
}

void EntityParam::setModel(const Model &model)
{
	_model = make_pair(true, model);
}

void EntityParam::setOrbit(const Orbit &orbit)
{
	_orbit = make_pair(true, orbit);
}

void EntityParam::setAtmo(const Atmo &atmo)
{
	_atmo = make_pair(true, atmo);
}

void EntityParam::setRing(const Ring &ring)
{
	_ring = make_pair(true, ring);
}

void EntityParam::setStar(const Star &star)
{
	_star = make_pair(true, star);
}

void EntityParam::setClouds(const Clouds &clouds)
{
	_clouds = make_pair(true, clouds);
}

void EntityParam::setNight(const Night &night)
{
	_night = make_pair(true, night);
}

void EntityParam::setSpecular(const Specular &specular)
{
	_specular = make_pair(true, specular);
}

bool EntityParam::hasOrbit() const
{
	return _orbit.first;
}

bool EntityParam::isBody() const
{
	return _model.first;
}

bool EntityParam::hasAtmo() const
{
	return _atmo.first;
}

bool EntityParam::hasRing() const
{
	return _ring.first;
}

bool EntityParam::isStar() const
{
	return _star.first;
}

bool EntityParam::hasClouds() const
{
	return _clouds.first;
}

bool EntityParam::hasNight() const
{
	return _night.first;
}

bool EntityParam::hasSpecular() const
{
	return _specular.first;
}

string EntityParam::getName() const
{
	return _name;
}

string EntityParam::getDisplayName() const
{
	return _displayName;
}

string EntityParam::getParentName() const
{
	return _parentName;
}

const Model &EntityParam::getModel() const
{
	return _model.second;
}

const Orbit &EntityParam::getOrbit() const
{
	return _orbit.second;
}

const Atmo &EntityParam::getAtmo() const
{
	return _atmo.second;
}

const Ring &EntityParam::getRing() const
{
	return _ring.second;
}

const Star &EntityParam::getStar() const
{
	return _star.second;
}

const Clouds &EntityParam::getClouds() const
{
	return _clouds.second;
}

const Night &EntityParam::getNight() const
{
	return _night.second;
}

const Specular &EntityParam::getSpecular() const
{
	return _specular.second;
}

EntityState::EntityState(
	const dvec3 &pos, float rotationAngle, float cloudDisp) :
	_position{pos},
	_rotationAngle{rotationAngle},
	_cloudDisp{cloudDisp}
{

}

dvec3 EntityState::getPosition() const
{
	return _position;
}

float EntityState::getRotationAngle() const
{
	return _rotationAngle;
}

float EntityState::getCloudDisp() const
{
	return _cloudDisp;
}

EntityHandle::EntityHandle(
	const EntityCollection* const collec, int id) :
	 _id{id}, _collec{collec}
{

}

bool EntityHandle::exists() const
{
	return _id != -1;
}

const EntityParam &EntityHandle::getParam() const
{
	return _collec->getParam(*this);
}

const EntityState &EntityHandle::getState() const
{
	return _collec->getState(*this);
}

bool EntityHandle::operator<(const EntityHandle &h) const
{
	return _id < h._id;
}

bool EntityHandle::operator==(const EntityHandle &h) const
{
	return _id == h._id;
}

void EntityCollection::init(const vector<EntityParam> &param)
{
	_param = param;
	_state.clear();
	_parents.resize(_param.size());
	// Assign parents
	for (size_t i=0;i<_param.size();++i)
	{
		const string parent = _param[i].getParentName();
		if (parent != "")
		{
			for (size_t j=0;j<_param.size();++j)
			{
				if (_param[j].getName() == parent)
				{
					if (i==j) 
						throw runtime_error("Entity " + parent + " Can be its own parent");
					_parents[i] = j;
					break;
				}
			}
		}
		else _parents[i] = -1;
	}

	_all.clear();
	_bodies.clear();
	// Categorization
	for (int i=0;i<(int)_param.size();++i)
	{
		const EntityHandle h = createHandle(i);
		_all.push_back(h);
		if (_param.at(i).isBody())
			_bodies.push_back(h);
	}
}

void EntityCollection::setState(const std::map<EntityHandle, EntityState> &state)
{
	_state.clear();
	_state.resize(_param.size());
	for (auto &p : state)
	{
		_state[p.first._id] = p.second;
	}
}

const vector<EntityHandle> &EntityCollection::getAll() const
{
	return _all;
}

const vector<EntityHandle> &EntityCollection::getBodies() const
{
	return _bodies;
}

EntityHandle EntityHandle::getParent() const
{
	if (!exists()) return {};
	return _collec->createHandle(_collec->_parents[_id]);
}

vector<EntityHandle> EntityHandle::getAllParents() const
{
	if (!exists()) return {};
	vector<EntityHandle> allParents = {};
	int temp = _id;
	int tempParent = -1;
	while ((tempParent = _collec->_parents[temp]) != -1)
	{
		allParents.push_back(_collec->createHandle(tempParent));
		temp = tempParent;
	}
	return allParents;
}

vector<EntityHandle> EntityHandle::getChildren() const
{
	if (!exists()) return {};
	vector<EntityHandle> children = {};
	for (size_t i=0;i<_collec->_parents.size();++i)
	{
		if (_collec->_parents[i] == _id) 
			children.push_back(_collec->createHandle(i));
	}
	return children;
}

vector<EntityHandle> EntityHandle::getAllChildren() const
{
	if (!exists()) return {};
	auto c = getChildren();
	vector<EntityHandle> accum = {};
	for (auto i : c)
	{
		auto cc = i.getAllChildren();
		accum.insert(accum.end(), cc.begin(), cc.end());
	}
	c.insert(c.end(), accum.begin(), accum.end());
	return c;
}

EntityHandle EntityCollection::createHandle(int id) const
{
	return EntityHandle(this, id);
}

const EntityParam &EntityCollection::getParam(const EntityHandle &handle) const
{
	if (!handle.exists()) throw runtime_error("Entity doesn't exist");
	return _param.at(handle._id);
}

const EntityState &EntityCollection::getState(const EntityHandle &handle) const
{
	if (!handle.exists()) throw runtime_error("Entity doesn't exist");
	return _state.at(handle._id);
}
