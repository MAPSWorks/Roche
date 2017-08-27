#pragma once

#include <vector>
#include <glm/glm.hpp>

struct Vertex
{
	glm::vec3 position;
	glm::vec2 uv;
	glm::vec3 normal;
};

typedef uint32_t Index;

class Mesh
{
public:
	Mesh() = default;
	Mesh(const std::vector<Vertex> &vertices, const std::vector<Index> &indices);
	const std::vector<Vertex> &getVertices() const;
	const std::vector<Index> &getIndices() const;
private:
	std::vector<Vertex> _vertices;
	std::vector<Index> _indices;
};

Mesh generateSphere(int meridians, int rings);

Mesh generateFlareMesh(int detail);

Mesh generateRingMesh(int meridians, float near, float far);