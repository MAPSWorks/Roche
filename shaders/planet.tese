layout(quads, fractional_even_spacing) in;

in vec3 gl_TessCoord;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(location = 0) in vec4 inPosition[gl_MaxPatchVertices];
layout(location = 1) in vec4 inUv[gl_MaxPatchVertices];
layout(location = 2) in vec4 inNormal[gl_MaxPatchVertices];
layout(location = 3) in vec4 inTangent[gl_MaxPatchVertices];

layout (binding = 0, std140) uniform sceneDynamicUBO
{
	SceneUBO sceneUBO;
};

layout (binding = 1, std140) uniform planetDynamicUBO
{
	PlanetUBO planetUBO;
};

#if defined(HAS_ATMO)
layout (binding = 6) uniform sampler2D atmo;
#endif

layout (location = 0) out vec4 passUv;
layout (location = 1) out vec4 passNormal;
layout (location = 2) out vec4 passPosition;
layout (location = 3) out vec4 passLocalPosition;
layout (location = 4) out vec4 passScattering;

vec4 lerp(vec4 v[gl_MaxPatchVertices])
{
	return mix(
		mix(v[0],v[1],gl_TessCoord.x),
		mix(v[2],v[3],gl_TessCoord.x),
		gl_TessCoord.y);
}

void main()
{
	passUv = lerp(inUv);
	mat4 mMat = getMatrix(planetUBO);
	passNormal = normalize(sceneUBO.viewMat*mMat*normalize(lerp(inNormal)));
	vec4 pos = lerp(inPosition);
#if !defined(IS_FAR_RING) && !defined(IS_NEAR_RING)
	pos = vec4(normalize(pos.xyz),1);
#endif
	passLocalPosition = mMat*pos;
	passPosition = sceneUBO.viewMat*passLocalPosition;
	gl_Position = sceneUBO.projMat*passPosition;
	// Logarithmic depth buffer
	gl_Position.z = logDepth(
		gl_Position.w, sceneUBO.logDepthFarPlane, sceneUBO.logDepthC);

#if defined(HAS_ATMO)
	vec3 viewer = -planetUBO.planetPos.xyz;
	vec3 view_dir = passPosition.xyz;
	vec3 scat = in_scattering_planet(viewer, view_dir,
		planetUBO.lightDir.xyz, planetUBO.radius, planetUBO.atmoHeight,
		atmo, planetUBO.K);
	passScattering = vec4(scat, 0.0);
#endif
}