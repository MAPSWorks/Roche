out gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
  float gl_ClipDistance[];
};

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inUv;
layout(location = 2) in vec4 inNormal;
layout(location = 3) in vec4 inTangent;

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	PlanetUBO planetUBO;
};

#if defined(HAS_ATMO)
layout (binding = 5) uniform sampler2D atmo;
#endif

layout (location = 0) out vec4 passUv;
layout (location = 1) out vec4 passNormal;
layout (location = 2) out vec4 passPosition;
#if defined(HAS_ATMO)
layout (location = 3) out vec4 passScattering;
#endif

void main(void)
{
	passUv = inUv;
	mat4 mMat = planetUBO.modelMat;
#if defined(IS_ATMO)
	mMat = planetUBO.atmoMat;
#endif
#if defined(IS_FAR_RING)
	mMat = planetUBO.ringFarMat;
#endif
#if defined(IS_NEAR_RING)
	mMat = planetUBO.ringNearMat;
#endif

	passNormal = normalize(sceneUBO.viewMat*mMat*inNormal);
	passPosition = sceneUBO.viewMat*mMat*inPosition;
	gl_Position = sceneUBO.projMat*passPosition;
	// Logarithmic depth buffer
	gl_Position.z = log2(max(1e-6, 1.0+gl_Position.w)) * (2.0/log2(5e10 + 1.0)) - 1.0;
	gl_Position.z *= gl_Position.w;

#if defined(HAS_ATMO)
	vec3 pp = passPosition.xyz-planetUBO.planetPos.xyz;
	vec3 viewer = sceneUBO.viewPos.xyz-planetUBO.planetPos.xyz;
	vec3 view_dir = pp-viewer;
	vec3 scat = in_scattering_planet(viewer, view_dir,
		planetUBO.lightDir.xyz, planetUBO.radius, planetUBO.atmoHeight,
		atmo, planetUBO.K);
	passScattering = vec4(scat, 0.0);
#endif
}