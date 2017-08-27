#include "mesh.hpp"

#include <glm/gtc/constants.hpp>
#include <cstring>

using namespace std;
using namespace glm;


Mesh::Mesh(
	const std::vector<Vertex> &vertices,
	const std::vector<Index> &indices) :
	_vertices{vertices},
	_indices{indices}
{

}

const std::vector<Vertex> &Mesh::getVertices() const
{
	return _vertices;
}

const std::vector<Index> &Mesh::getIndices() const
{
	return _indices;
}

Mesh generateSphere(
	const int meridians, 
	const int rings)
{
	// Vertices
	vector<Vertex> vertices((meridians+1)*(rings+1));
	size_t offset = 0;
	for (int i=0;i<=rings;++i)
	{
		const float phi = glm::pi<float>()*((float)i/(float)rings-0.5);
		const float cp = cos(phi);
		const float sp = sin(phi);
		for (int j=0;j<=meridians;++j)
		{
			const float theta = 2*glm::pi<float>()*((float)j/(float)meridians);
			const float ct = cos(theta);
			const float st = sin(theta);
			const vec3 pos = vec3(cp*ct,cp*st,sp);
			const vec2 uv = vec2(j/(float)meridians, 1.f-i/(float)rings);
			const vec3 normal = normalize(pos);
			vertices[offset] = {
				pos,
				uv,
				normal
			};
			offset++;
		}
	}

	// Indices
	vector<Index> indices(meridians*rings*4);
	offset = 0;
	for (int i=0;i<rings;++i)
	{
		for (int j=0;j<meridians;++j)
		{
			const Index i1 = i+1;
			const Index j1 = j+1;
			vector<Index> ind = {
				(Index)(i *(rings+1)+j),
				(Index)(i *(rings+1)+j1),
				(Index)(i1*(rings+1)+j),
				(Index)(i1*(rings+1)+j1)
			};
			// Copy to indices
			memcpy(&indices[offset], ind.data(), ind.size()*sizeof(Index));
			offset += 4;
		}
	}
	return Mesh(vertices, indices);
}

Mesh generateFlareMesh(const int detail)
{
	vector<Vertex> vertices((detail+1)*2);

	for (int i=0;i<=detail;++i)
	{
		const float f = i/(float)detail;
		const vec2 pos = vec2(cos(f*2*glm::pi<float>()),sin(f*2*glm::pi<float>()));
		vertices[i*2+0] = {vec3(0,0,0), vec2(0.5, 0.5)};
		vertices[i*2+1] = {vec3(pos,0), vec2(pos*vec2(0.5)+vec2(0.5))};
	}

	vector<Index> indices(detail*6);
	for (int i=0;i<detail;++i)
	{
		indices[i*6+0] = (i*2)+0;
		indices[i*6+1] = (i*2)+1;
		indices[i*6+2] = (i*2)+2;
		indices[i*6+3] = (i*2)+2;
		indices[i*6+4] = (i*2)+1;
		indices[i*6+5] = (i*2)+3;
	}
	return Mesh(vertices, indices);
}

Mesh generateRingMesh(
	const int meridians,
	const float near,
	const float far)
{
	vector<Vertex> vertices((meridians+1)*2);
	{
		int offset = 0;
		for (int i=0;i<=meridians;++i)
		{
			float angle = (glm::pi<float>()*i)/(float)meridians;
			vec2 pos = vec2(cos(angle),sin(angle));
			vertices[offset+0] = {vec3(pos*near, 0.0), vec2(pos*1.f)};
			vertices[offset+1] = {vec3(pos*far , 0.0), vec2(pos*2.f)};
			offset += 2;
		}
	}

	vector<Index> indices(meridians*4);
	{
		int offset = 0;
		Index vert = 0;
		for (int i=0;i<meridians;++i)
		{
			indices[offset+0] = vert+0;
			indices[offset+1] = vert+1;
			indices[offset+2] = vert+2;
			indices[offset+3] = vert+3;
			offset += 4;
			vert += 2; 
		}
	}
	return Mesh(vertices, indices);
}
