#pragma once
#include "Mesh.h"
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
using namespace DirectX;
class Entity {
public:
	Entity(Mesh *m);
	~Entity();
	void PositionDelta(XMFLOAT3 delta);
	void SetPosition(XMFLOAT3 pos);
	void SetRotation(XMFLOAT4X4 rot);
	void SetScale(XMFLOAT3 sc);
	XMMATRIX GetWorldMatrix();
	Mesh* GetMesh();
private:
	XMFLOAT4X4 worldMatrix;
	bool wmDirty;
	Mesh* mesh;
	XMFLOAT3 position;
	XMFLOAT4X4 rotation;
	XMFLOAT3 scale;
};