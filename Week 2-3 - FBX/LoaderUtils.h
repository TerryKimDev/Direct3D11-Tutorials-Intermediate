#pragma once

#include <iostream>
#include <directxmath.h>
#include <vector>

// FBX includes
#include <fbxsdk.h>
#include "MeshUtils.h"
#include <string>

FbxManager* gSdkManager;

// funtime random normal
#define RAND_NORMAL XMFLOAT3(rand()/float(RAND_MAX),rand()/float(RAND_MAX),rand()/float(RAND_MAX))

void InitFBX()
{
	gSdkManager = FbxManager::Create();

	// create an IOSettings object
	FbxIOSettings* ios = FbxIOSettings::Create(gSdkManager, IOSROOT);
	gSdkManager->SetIOSettings(ios);
}

void LoadFBX(const std::string& filename, SimpleMesh<SimpleVertex> &simpleMesh, float scale)
{
	const char* ImportFileName = filename.c_str(); 

	// Create a scene
	FbxScene* lScene = FbxScene::Create(gSdkManager, "");

	FbxImporter* lImporter = FbxImporter::Create(gSdkManager, "");

	// Initialize the importer by providing a filename.
	if (!lImporter->Initialize(ImportFileName, -1, gSdkManager->GetIOSettings())) {
		printf("Call to FbxImporter::Initialize() failed.\n");
		printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString());
		//exit(-1);
	}

	// Import the scene.
	bool lStatus = lImporter->Import(lScene);

	// Destroy the importer
	lImporter->Destroy();

	// Process the scene and build DirectX Arrays
	//ProcessFBXMesh(lScene->GetRootNode(), simpleMesh, scale);

	// Optimize the mesh
	//MeshUtils::Compactify(simpleMesh);

	// Destroy the (no longer needed) scene
	lScene->Destroy();
}