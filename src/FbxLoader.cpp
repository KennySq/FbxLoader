#include"FbxLoader.h"

#include<iostream> 

FbxLoader::FbxLoader(const char* path) noexcept
	:mPath(path)
{
	fbx_loadFbx();
}

bool FbxLoader::fbx_loadFbx()
{
	FbxManager* manager = FbxManager::Create();
	FbxIOSettings* io = FbxIOSettings::Create(manager, IOSROOT);

	manager->SetIOSettings(io);

	FbxImporter* importer = FbxImporter::Create(manager, "");

	const char* path = mPath.c_str();

	bool result = importer->Initialize(path, -1, manager->GetIOSettings());

	if (result == false)
	{
		auto errorStatus = importer->GetStatus();
		return false;
	}

	FbxScene* scene = FbxScene::Create(manager, "Scene");
	importer->Import(scene);

	fbxsdk::FbxAxisSystem::DirectX.ConvertScene(scene);

	FbxGeometryConverter geometryConverter(manager);
	geometryConverter.Triangulate(scene, true);


	auto nodeCount = scene->GetNodeCount();
	FbxNode* rootNode = scene->GetRootNode();

	fbx_loadNode(rootNode);

	importer->Destroy();

	return true;
}
void FbxLoader::fbx_loadNode(FbxNode* node)
{
	FbxNodeAttribute* nodeAttribute = node->GetNodeAttribute();
	FbxAMatrix transform = node->EvaluateLocalTransform();
	if (nodeAttribute != nullptr)
	{
		FbxNodeAttribute::EType attr = nodeAttribute->GetAttributeType();
		if (attr == FbxNodeAttribute::eMesh)
		{


			FbxMesh* mesh = node->GetMesh();
			int deformCount = mesh->GetDeformerCount();

			for (unsigned int i = 0; i < deformCount; i++)
			{

				FbxDeformer* deform = mesh->GetDeformer(i, FbxDeformer::eSkin);
				FbxSkin* skin = reinterpret_cast<FbxSkin*>(deform);
				if (skin == nullptr)
				{
					continue;
				}

				int clusterCount = skin->GetClusterCount();

				for (unsigned int j = 0; j < clusterCount; j++)
				{
					FbxCluster* cluster = skin->GetCluster(j);

					std::string jointName = cluster->GetLink()->GetName();
					FbxAMatrix transformMatrix;
					FbxAMatrix transformLinkMatrix;
					FbxAMatrix globalInverseMatrix;

					cluster->GetTransformMatrix(transformMatrix);
					cluster->GetTransformLinkMatrix(transformLinkMatrix);
					globalInverseMatrix = transformLinkMatrix.Inverse();

					


				}
			}
			fbx_getControlPoints(mesh);

			unsigned int triCount = mesh->GetPolygonCount();
			unsigned int vertexCount = 0;

			mTotalTriangles += triCount;
			unsigned int deformCount = mesh->GetDeformerCount();

			for (unsigned int i = 0; i < triCount; i++)
			{
				unsigned int polyVertexCount = mesh->GetPolygonSize(i);

				for (unsigned int j = 0; j < polyVertexCount; j++)
				{
					int controlPointIndex = mesh->GetPolygonVertex(i, j);
					
					XMFLOAT3 normal{};
					XMFLOAT3 binormal{};
					XMFLOAT3& position = mPositions[controlPointIndex];
					XMFLOAT3 tangent{};
					XMFLOAT2 uv{};
					unsigned int materialID = 0;

					if (mesh->GetElementNormalCount() >= 1)
					{
						normal = fbx_getNormal(mesh, controlPointIndex, vertexCount);
					}

					if (mesh->GetElementBinormalCount() >= 1)
					{
						binormal = fbx_getBinormals(mesh, controlPointIndex, vertexCount);
					}

					if (mesh->GetElementTangentCount() >= 1)
					{
						tangent = fbx_getTangents(mesh, controlPointIndex, vertexCount);
					}

					if (mesh->GetElementUVCount() >= 1)
					{
						uv = fbx_getTexcoords(mesh, controlPointIndex, i * polyVertexCount + j);
					}

					if (mesh->GetElementMaterialCount() >= 1)
					{
						materialID = fbx_getMaterialID(mesh, i);
					}

					Vertex vertex;


					vertex.mPosition = position;
					vertex.mNormal = normal;
					vertex.mBinormal = binormal;
					vertex.mTangent = tangent;
					vertex.mTexcoord = uv;
					vertex.mMaterialID = materialID;

					fbx_insertVertex(position, normal, binormal, tangent, uv, materialID, vertexCount);
					vertexCount++;


				}
			}
			mPositions.clear();
			mMeshCount++;
		}
	}

	const int childCount = node->GetChildCount();

	for (unsigned int i = 0; i < childCount; i++)
	{
		fbx_loadNode(node->GetChild(i));
	}

}
void FbxLoader::fbx_getControlPoints(FbxMesh* mesh)
{
	unsigned int count = mesh->GetControlPointsCount();

	for (unsigned int i = 0; i < count; i++)
	{
		XMFLOAT3 position;
		
		position.x = static_cast<float>(mesh->GetControlPointAt(i).mData[0]);
		position.y = static_cast<float>(mesh->GetControlPointAt(i).mData[1]);
		position.z = static_cast<float>(mesh->GetControlPointAt(i).mData[2]);
		
		mPositions.emplace_back(position);
	}
}

void FbxLoader::fbx_toVertex(const XMFLOAT3& position, const XMFLOAT3& normal, const XMFLOAT3& binormal, const XMFLOAT3& tangent, const XMFLOAT2& texcoord)
{

}

XMFLOAT3 FbxLoader::fbx_getNormal(const FbxMesh* mesh, int controlPoint, int vertex)
{
	XMFLOAT3 result = XMFLOAT3(0, 0, 0);

	if (mesh->GetElementNormalCount() < 1)
	{
		return result;
	}

	const FbxGeometryElementNormal* vertexNormal = mesh->GetElementNormal(0);

	switch (vertexNormal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controlPoint).mData[0]);
			result.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controlPoint).mData[1]);
			result.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controlPoint).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(controlPoint);

			result.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		}
	}
	break;

	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertex).mData[0]);
			result.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertex).mData[1]);
			result.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertex).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(vertex);
			result.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		}
	}
	break;

	}

	mNormals.emplace_back(result);

	return result;

}

XMFLOAT3 FbxLoader::fbx_getBinormals(const FbxMesh* mesh, int controlPoint, int vertex)
{
	XMFLOAT3 result = XMFLOAT3(0, 0, 0);

	if (mesh->GetElementBinormalCount() < 1)
	{
		return result;
	}

	const FbxGeometryElementBinormal* vertexBinormal = mesh->GetElementBinormal(0);

	switch (vertexBinormal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{
		switch (vertexBinormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controlPoint).mData[0]);
			result.y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controlPoint).mData[1]);
			result.z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controlPoint).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexBinormal->GetIndexArray().GetAt(controlPoint);

			result.x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		}
	}
	break;

	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (vertexBinormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertex).mData[0]);
			result.y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertex).mData[1]);
			result.z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertex).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexBinormal->GetIndexArray().GetAt(vertex);
			result.x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		}
	}
	break;

	}

	mBinormals.emplace_back(result);

	return result;

}

XMFLOAT3 FbxLoader::fbx_getTangents(const FbxMesh* mesh, int controlPoint, int vertex)
{
	XMFLOAT3 result = XMFLOAT3(0, 0, 0);

	if (mesh->GetElementTangentCount() < 1)
	{
		return result;
	}

	const FbxGeometryElementTangent* vertexTangent = mesh->GetElementTangent(0);

	switch (vertexTangent->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{
		switch (vertexTangent->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controlPoint).mData[0]);
			result.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controlPoint).mData[1]);
			result.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controlPoint).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexTangent->GetIndexArray().GetAt(controlPoint);

			result.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		}
	}
	break;

	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (vertexTangent->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertex).mData[0]);
			result.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertex).mData[1]);
			result.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertex).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexTangent->GetIndexArray().GetAt(vertex);
			result.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		}
	}
	break;

	}

	mTangents.emplace_back(result);

	return result;

}

XMFLOAT2 FbxLoader::fbx_getTexcoords(const FbxMesh* mesh, int controlPoint, int texcoordIndex)
{
	XMFLOAT2 result = XMFLOAT2(0, 0);

	if (mesh->GetElementUVCount() < 1)
	{
		return result;
	}

	const FbxGeometryElementUV* vertexUV = mesh->GetElementUV(0);

	switch (vertexUV->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{
		switch (vertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(controlPoint).mData[0]);
			result.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(controlPoint).mData[1]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexUV->GetIndexArray().GetAt(controlPoint);

			result.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[1]);
		}
		break;

		}
	}
	break;

	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (vertexUV->GetReferenceMode())
		{

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexUV->GetIndexArray().GetAt(texcoordIndex);
			result.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[0]);
			result.y = 1.0f - static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[1]);
		}
		break;
		}
	}
	break;

	}

	return result;
}

unsigned int FbxLoader::fbx_getMaterialID(const FbxMesh* mesh, int polygon)
{
	unsigned int result = 0;

	for (unsigned int i = 0; i < mesh->GetLayerCount(); i++)
	{
		const fbxsdk::FbxLayerElementMaterial* layerMat = mesh->GetLayer(i)->GetMaterials();

		if (layerMat == nullptr)
		{
			continue;
		}

		result = layerMat->GetIndexArray().GetAt(polygon);
	}

	mMaterialIDs.emplace_back(result);

	return result;

}

void FbxLoader::fbx_insertVertex(const XMFLOAT3& position, const XMFLOAT3& normal, const XMFLOAT3& binormal, const XMFLOAT3& tangent, const XMFLOAT2& uv, int materialID, unsigned int vertexCount)
{
	Vertex vertex = { position , normal, binormal, tangent, uv, materialID };

	auto lookup = mIndexMap.find(vertex);

	if (lookup != mIndexMap.end())
	{
		Indices.push_back(lookup->second);
	}
	else
	{
		unsigned int index = Vertices.size();
		mIndexMap[vertex] = index;
		Indices.push_back(index);
		Vertices.push_back(vertex);
	}
}
