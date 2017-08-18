layout(quads, fractional_even_spacing) in;

in vec3 gl_TessCoord;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(location = 0) in vec3 inPosition[gl_MaxPatchVertices];
layout(location = 1) in vec2 inUv[gl_MaxPatchVertices];
layout(location = 2) in vec3 inNormal[gl_MaxPatchVertices];

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

layout (location = 0) out vec3 passPosition;
layout (location = 1) out vec2 passUv;
layout (location = 2) out vec3 passNormal;
layout (location = 3) out vec3 passScattering;

void main()
{
	passUv = lerp(inUv, gl_TessCoord);
	mat4 mMat = getMatrix(planetUBO);
	passNormal = normalize(vec3(
		sceneUBO.viewMat*mMat*vec4(normalize(lerp(inNormal, gl_TessCoord)),0)));
	vec3 pos = lerp(inPosition, gl_TessCoord);
#if !defined(IS_FAR_RING) && !defined(IS_NEAR_RING)
	pos = normalize(pos);
#endif
	vec4 localPos = mMat*vec4(pos,1);
	passPosition = vec3(sceneUBO.viewMat*localPos);
	gl_Position = sceneUBO.projMat*vec4(passPosition,1);
	// Logarithmic depth buffer
	gl_Position.z = logDepth(
		gl_Position.w, sceneUBO.logDepthFarPlane, sceneUBO.logDepthC);

#if defined(HAS_ATMO)
	float dist = length(passPosition);
	vec3 view_dir = -passPosition/dist;
	passScattering = in_scattering_planet(
		passPosition-planetUBO.planetPos.xyz, view_dir, dist,
		planetUBO.lightDir.xyz, planetUBO.radius, planetUBO.atmoHeight,
		atmo, planetUBO.K);
#endif
}