#include "Game.h"
#include "Vertex.h"
#include "WICTextureLoader.h"
#define WINDOWWIDTH 1200
#define WINDOWHEIGHT 960
// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore( 
		hInstance,		   // The application's handle
		"DirectX Game",			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{
	// Initialize fields
	//vertexBuffer = 0;
	//indexBuffer = 0;
	entities = std::vector<Entity*>();
	vertexShader = 0;
	pixelShader = 0;
	mainCamera = new Camera(XMFLOAT3(0,0,-10));
	mainCamera->CreateProjectionMatrix(width, height, 80);
	SetCursorPos(width / 2, height / 2);
	prevMousePos.x = 0;
	prevMousePos.y = 0;

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.");
#endif
}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Release any (and all!) DirectX objects
	// we've made in the Game class
	//if (vertexBuffer) { vertexBuffer->Release(); }
	//if (indexBuffer) { indexBuffer->Release(); }

	// Delete our simple shader objects, which
	// will clean up their own internal DirectX stuff
	for (Entity* e : entities)
		delete e;
	for (ID3D11ShaderResourceView * srv : textureViews)
		srv->Release();
	for (ID3D11SamplerState *ss : samplerStates)
		ss->Release();
	delete vertexShader;
	delete pixelShader;
	delete mainCamera;
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	LoadShaders();
	CreateMatrices();
	CreateBasicGeometry();

	dLight = DirectionalLight();
	dLight.AmbientColor = XMFLOAT4(0.1, 0, 0.1, 1.0);
	dLight.DiffuseColor = XMFLOAT4(1, 1, 1, 1);
	dLight.Direction = XMFLOAT3(1, 1, 1);

	dLight2 = DirectionalLight();
	dLight2.AmbientColor = XMFLOAT4(0, 0.15, 0.1, 1.0);
	dLight2.DiffuseColor = XMFLOAT4(1, 1, 1, 1);
	dLight2.Direction = XMFLOAT3(-1, -1, -1);

	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files using
// my SimpleShader wrapper for DirectX shader manipulation.
// - SimpleShader provides helpful methods for sending
//   data to individual variables on the GPU
// --------------------------------------------------------
void Game::LoadShaders()
{
	vertexShader = new SimpleVertexShader(device, context);
	if (!vertexShader->LoadShaderFile(L"Debug/VertexShader.cso"))
		vertexShader->LoadShaderFile(L"VertexShader.cso");		

	pixelShader = new SimplePixelShader(device, context);
	if(!pixelShader->LoadShaderFile(L"Debug/PixelShader.cso"))	
		pixelShader->LoadShaderFile(L"PixelShader.cso");

	// You'll notice that the code above attempts to load each
	// compiled shader file (.cso) from two different relative paths.

	// This is because the "working directory" (where relative paths begin)
	// will be different during the following two scenarios:
	//  - Debugging in VS: The "Project Directory" (where your .cpp files are) 
	//  - Run .exe directly: The "Output Directory" (where the .exe & .cso files are)

	// Checking both paths is the easiest way to ensure both 
	// scenarios work correctly, although others exist
}



// --------------------------------------------------------
// Initializes the matrices necessary to represent our geometry's 
// transformations and our 3D camera
// --------------------------------------------------------
void Game::CreateMatrices()
{
	// Set up world matrix
	// - In an actual game, each object will need one of these and they should
	//   update when/if the object moves (every frame)
	// - You'll notice a "transpose" happening below, which is redundant for
	//   an identity matrix.  This is just to show that HLSL expects a different
	//   matrix (column major vs row major) than the DirectX Math library
	//XMMATRIX W = XMMatrixIdentity();
	//XMStoreFloat4x4(&worldMatrix, XMMatrixTranspose(W)); // Transpose for HLSL!

	// Create the View matrix
	// - In an actual game, recreate this matrix every time the camera 
	//    moves (potentially every frame)
	// - We're using the LOOK TO function, which takes the position of the
	//    camera and the direction vector along which to look (as well as "up")
	// - Another option is the LOOK AT function, to look towards a specific
	//    point in 3D space
	XMVECTOR pos = XMVectorSet(0, 0, -5, 0);
	XMVECTOR dir = XMVectorSet(0, 0, 1, 0);
	XMVECTOR up  = XMVectorSet(0, 1, 0, 0);
	XMMATRIX V   = XMMatrixLookToLH(
		pos,     // The position of the "camera"
		dir,     // Direction the camera is looking
		up);     // "Up" direction in 3D space (prevents roll)
	//XMStoreFloat4x4(&viewMatrix, XMMatrixTranspose(V)); // Transpose for HLSL!

	// Create the Projection matrix
	// - This should match the window's aspect ratio, and also update anytime
	//   the window resizes (which is already happening in OnResize() below)
	XMMATRIX P = XMMatrixPerspectiveFovLH(
		0.25f * 3.1415926535f,		// Field of View Angle
		(float)width / height,		// Aspect ratio
		0.1f,						// Near clip plane distance
		100.0f);					// Far clip plane distance
	//XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(P)); // Transpose for HLSL!
}


// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	// Create some temporary variables to represent colors
	// - Not necessary, just makes things more readable
	XMFLOAT4 red	= XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4 green	= XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
	XMFLOAT4 blue	= XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);

	// Set up the vertices of the triangle we would like to draw
	// - We're going to copy this array, exactly as it exists in memory
	//    over to a DirectX-controlled data structure (the vertex buffer)
	Vertex vertices[] = 
	{
		{ XMFLOAT3(+0.0f, +1.0f, 1.0f), XMFLOAT3(0,0,1), XMFLOAT2(0,0) },
		{ XMFLOAT3(+1.5f, -1.0f, 1.0f), XMFLOAT3(0,0,1), XMFLOAT2(0,0) },
		{ XMFLOAT3(-1.5f, -1.0f, 1.0f), XMFLOAT3(0,0,1), XMFLOAT2(0,0) },
	};

	// Set up the indices, which tell us which vertices to use and in which order
	// - This is somewhat redundant for just 3 vertices (it's a simple example)
	// - Indices are technically not required if the vertices are in the buffer 
	//    in the correct order and each one will be used exactly once
	// - But just to see how it's done...
	unsigned int indices[] = { 0, 1, 2 };


	ID3D11ShaderResourceView * textureView;
	CreateWICTextureFromFile(device, context, L"assets\\artsy hairtie.png", nullptr, &textureView);
	ID3D11SamplerState * samplerState;
	D3D11_SAMPLER_DESC samplerStateDesc = {};
	samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerStateDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&samplerStateDesc, &samplerState);
	textureViews.push_back(textureView);
	samplerStates.push_back(samplerState);
	//entities.push_back(new Entity(new Mesh(vertices, sizeof(vertices) / sizeof(Vertex), indices, sizeof(indices) / sizeof(unsigned int), device), new Material(vertexShader, pixelShader)));
	entities.push_back(new Entity(new Mesh("assets\\cube.obj", device), new Material(vertexShader, pixelShader, textureView, samplerState), XMFLOAT3(0, 0, 0)));
	entities.push_back(new Entity(new Mesh("assets\\sphere.obj", device), new Material(vertexShader, pixelShader, textureView, samplerState), XMFLOAT3(5, 0, 0)));

	/*Vertex vertices2[] =
	{
		{ XMFLOAT3(+0.0f, 0.0f, +1.0f), red },
		{ XMFLOAT3(+1.5f, -2.0f, +1.0f), blue },
		{ XMFLOAT3(-1.5f, -2.0f, +1.0f), green },
	};

	// Set up the indices, which tell us which vertices to use and in which order
	// - This is somewhat redundant for just 3 vertices (it's a simple example)
	// - Indices are technically not required if the vertices are in the buffer 
	//    in the correct order and each one will be used exactly once
	// - But just to see how it's done...
	unsigned int indices2[] = { 0, 1, 2 };

	meshes.push_back(new Mesh(vertices2, sizeof(vertices2) / sizeof(Vertex), indices2, sizeof(indices2) / sizeof(unsigned int), device));

	Vertex vertices3[] =
	{
		{ XMFLOAT3(+0.0f, 2.0f, +2.0f), red },
		{ XMFLOAT3(+1.5f, 0.0f, +2.0f), blue },
		{ XMFLOAT3(-1.5f, 0.0f, +2.0f), green },
	};

	// Set up the indices, which tell us which vertices to use and in which order
	// - This is somewhat redundant for just 3 vertices (it's a simple example)
	// - Indices are technically not required if the vertices are in the buffer 
	//    in the correct order and each one will be used exactly once
	// - But just to see how it's done...
	unsigned int indices3[] = { 0, 1, 2 };

	meshes.push_back(new Mesh(vertices3, sizeof(vertices3) / sizeof(Vertex), indices3, sizeof(indices3) / sizeof(unsigned int), device));*/
}


// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update our projection matrix since the window size changed
	XMMATRIX P = XMMatrixPerspectiveFovLH(
		0.25f * 3.1415926535f,	// Field of View Angle
		(float)width / height,	// Aspect ratio
		0.1f,				  	// Near clip plane distance
		100.0f);			  	// Far clip plane distance
	XMStoreFloat4x4(&projectionMatrix, XMMatrixTranspose(P)); // Transpose for HLSL!
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Quit if the escape key is pressed
	if (GetAsyncKeyState(VK_ESCAPE))
		Quit();
	//entities[0]->PositionDelta(XMFLOAT3(1*deltaTime, 0, 0));
	if (GetAsyncKeyState('W') & 0x8000)
	{
		mainCamera->RelativePositionDelta(0, 0, 1*deltaTime);
	}
	if (GetAsyncKeyState('A') & 0x8000)
	{
		//mainCamera->RotationDelta(0, -1 * deltaTime);
		mainCamera->RelativePositionDelta(-1 * deltaTime, 0, 0);
	}
	if (GetAsyncKeyState('S') & 0x8000)
	{
		mainCamera->RelativePositionDelta(0, 0, -1*deltaTime);
	}
	if (GetAsyncKeyState('D') & 0x8000)
	{
		mainCamera->RelativePositionDelta(1 * deltaTime, 0, 0);
	}
	mainCamera->Update();
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = {0.4f, 0.6f, 0.75f, 0.0f};

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV, color);
	context->ClearDepthStencilView(
		depthStencilView, 
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

	for (Entity* e : entities)
	{
		e->PrepareMaterial(mainCamera->GetView(), mainCamera->GetProjection(), dLight, dLight2);
		Mesh *m = e->GetMesh();
		// Set buffers in the input assembler
		//  - Do this ONCE PER OBJECT you're drawing, since each object might
		//    have different geometry.
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		ID3D11Buffer * vb = m->GetVertexBuffer();
		context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
		context->IASetIndexBuffer(m->GetIndexBuffer(), DXGI_FORMAT_R32_UINT, 0);

		// Finally do the actual drawing
		//  - Do this ONCE PER OBJECT you intend to draw
		//  - This will use all of the currently set DirectX "stuff" (shaders, buffers, etc)
		//  - DrawIndexed() uses the currently set INDEX BUFFER to look up corresponding
		//     vertices in the currently set VERTEX BUFFER
		context->DrawIndexed(
			e->GetMesh()->GetIndexCount(),     // The number of indices to use (we could draw a subset if we wanted)
			0,     // Offset to the first index we want to use
			0);    // Offset to add to each index when looking up vertices
	}

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);
}


#pragma region Mouse Input

// --------------------------------------------------------
// Helper method for mouse clicking.  We get this information
// from the OS-level messages anyway, so these helpers have
// been created to provide basic mouse input if you want it.
// --------------------------------------------------------
void Game::OnMouseDown(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...


	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;

	// Caputure the mouse so we keep getting mouse move
	// events even if the mouse leaves the window.  we'll be
	// releasing the capture once a mouse button is released
	SetCapture(hWnd);
}

// --------------------------------------------------------
// Helper method for mouse release
// --------------------------------------------------------
void Game::OnMouseUp(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...

	// We don't care about the tracking the cursor outside
	// the window anymore (we're not dragging if the mouse is up)
	ReleaseCapture();
}

// --------------------------------------------------------
// Helper method for mouse movement.  We only get this message
// if the mouse is currently over the window, or if we're 
// currently capturing the mouse.
// --------------------------------------------------------
void Game::OnMouseMove(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...
	if (!firstMouse) mainCamera->RotationDelta((y - prevMousePos.y) *.01, (x - prevMousePos.x)*.01);
	else
		firstMouse = false;
	//SetCursorPos(windowhei)
	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;

	
}

// --------------------------------------------------------
// Helper method for mouse wheel scrolling.  
// WheelDelta may be positive or negative, depending 
// on the direction of the scroll
// --------------------------------------------------------
void Game::OnMouseWheel(float wheelDelta, int x, int y)
{
	// Add any custom code here...
}
#pragma endregion