//------------------------------------------------------------------------------
// EditorApp.cpp
//
// Nightbloom Editor Application - Fixed for ImGui compatibility
// Copyright (c) 2024. All rights reserved.
//------------------------------------------------------------------------------

#include "Engine/Core/Application.hpp"
#include "Engine/Core/Logger/Logger.hpp"
#include "Engine/Renderer/PipelineInterface.hpp"  // For pipeline management
#include "Engine/Renderer/DrawCommandSystem.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Tools/ShaderEditor/ShaderNodeEditor.hpp"
#include "EditorFileUtils.hpp"
#include <imgui.h>
#include <memory>
#include <filesystem>

#include "Engine/Renderer/Components/ResourceManager.hpp"
#include "Engine/Renderer/GLTFLoader.hpp"
#include "Engine/Renderer/Vulkan/VulkanShader.hpp"
#include "Engine/Renderer/Vulkan/VulkanPipeline.hpp"
#include "Engine/Renderer/Vulkan/VulkanPipelineAdapter.hpp"
#include "Engine/Renderer/Model.hpp"
#include "Engine/Renderer/Material.hpp"

#include "Engine/Renderer/Camera.hpp"

#include "Engine/Renderer/AssetManager.hpp"
// Check if docking is available
#ifdef IMGUI_HAS_DOCK
#define USE_IMGUI_DOCKING 1
#else
#define USE_IMGUI_DOCKING 0
#endif

namespace Nightbloom {

	class EditorApplication : public Application
	{
	public:
		EditorApplication() : Application("Nightbloom Editor")
		{
			LOG_INFO("=== Nightbloom Editor Starting ===");

			SetupDefaultProject();
		}

		~EditorApplication()
		{
			SaveEditorSettings();

			LOG_INFO("=== Nightbloom Editor Shutdown Complete ===");
		}

		void OnStartup() override
		{
			LOG_INFO("Editor application started");

			if (GetWindow())
			{
				UpdateWindowTitle();
				//GetWindow()->Maximize();
			}

			// Set up Camera
			m_ViewMatrix = glm::lookAt(
				glm::vec3(3.0f, 3.0f, 3.0f),  // Camera position
				glm::vec3(0.0f, 0.0f, 0.0f),  // Look at origin
				glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
				);

			// Set up Projection Matrix
			float aspect = 1280 / 720.0f; // Assuming a 16:9 aspect ratio TODO: make this dynamic
			m_ProjectionMatrix = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
			m_ProjectionMatrix[1][1] *= -1;  // Vulkan correction

			// Pass to renderer
			GetRenderer()->SetViewMatrix(m_ViewMatrix);
			GetRenderer()->SetProjectionMatrix(m_ProjectionMatrix);

			// In OnStartup()
			m_Camera = std::make_unique<Camera>();
			m_Camera->SetPosition(glm::vec3(3.0f, 3.0f, 3.0f));
			m_Camera->SetRotation(-135.0f, -20.0f);  // Look toward origin
			m_Camera->SetPerspective(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);

			m_Scene = std::make_unique<TestScene>();

			// Create test cube drawable
			// Assuming the renderer already created vertex/index buffers
			// You'd get these from the renderer or create them here
			auto* vertexBuffer = GetRenderer()->GetTestVertexBuffer();  // Add getter
			auto* indexBuffer = GetRenderer()->GetTestIndexBuffer();    // Add getter
			uint32_t indexCount = GetRenderer()->GetTestIndexCount();   // Add getter

			TestGLTFLoader();

			LoadToyCar();


			ResourceManager* resources = GetRenderer()->GetResourceManager();

			if (vertexBuffer && indexBuffer && indexCount > 0)
			{
				// Create test cube drawable
				m_TestCube = std::make_unique<MeshDrawable>(
					vertexBuffer, indexBuffer, indexCount,
					m_CurrentTestPipeline
				);
				m_TestCube->SetTransform(glm::mat4(1.0f));

				//m_TestCube->SetViewMatrix(m_ViewMatrix);
				//m_TestCube->SetProjectionMatrix(m_ProjectionMatrix);

				if (resources)
				{
					VulkanTexture* checkerTexture = resources->GetTexture("uv_checker");
					if (checkerTexture)
					{
						m_TestCube->AddTexture(checkerTexture);
						LOG_INFO("Cube 1: Added UV checker texture");
					}
				}

				m_TestCube2 = std::make_unique<MeshDrawable>(
					vertexBuffer,    // SAME geometry as cube 1!
					indexBuffer,     // SAME geometry as cube 1!
					indexCount,      // SAME index count!
					PipelineType::Mesh  // Use Mesh pipeline (has depth enabled!)
				);

				glm::mat4 cube2Transform = glm::translate(glm::mat4(1.0f), glm::vec3(.50f, 0.2f, -1.0f));
				m_TestCube2->SetTransform(cube2Transform);

				if (resources)
				{
					VulkanTexture* whiteTexture = resources->GetTexture("default_white");
					if (whiteTexture)
					{
						m_TestCube2->AddTexture(whiteTexture);
						LOG_INFO("Cube 2: Added white texture");
					}
				}

				LOG_INFO("Created 2 cubes sharing the same vertex/index buffers");
			}
			else
			{
				LOG_ERROR("Test geometry not available! vertexBuffer={}, indexBuffer={}, indexCount={}",
					(void*)vertexBuffer, (void*)indexBuffer, indexCount);
			}

			// Enable docking if available
			#if USE_IMGUI_DOCKING
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
			#endif

			m_ShaderNodeEditor = std::make_unique<ShaderNodeEditor>();

		}

		void OnUpdate(float deltaTime) override
		{
			// Update editor systems
			m_FrameTime += deltaTime;
			m_FrameCount++;

			// Update title bar with stats every second
			if (m_FrameTime >= 1.0f)
			{
				float fps = m_FrameCount / m_FrameTime;
				m_FPS = fps;
				//std::string title = "Nightbloom Editor - FPS: " + std::to_string(static_cast<int>(fps));
				//GetWindow()->SetTitle(title);
				UpdateWindowTitle();
				m_FrameTime = 0.0f;
				m_FrameCount = 0;
			}

			// Camera controls (only when right mouse button held)
			if (GetInput()->IsDown(InputCode::Mouse_Right))
			{
				if (!m_CameraControlActive)
				{
					m_CameraControlActive = true;
					// Optionally hide/capture cursor here
				}

				// Rotation from mouse delta
				glm::vec2 mouseDelta = glm::vec2(GetInput()->GetMouseDeltaX(), GetInput()->GetMouseDeltaY());
				m_Camera->SetRotationInput(glm::vec2(mouseDelta.x, -mouseDelta.y));

				// Movement from WASD
				glm::vec3 moveInput(0.0f);
				if (GetInput()->IsDown(InputCode::Key_W)) moveInput.z += 1.0f;
				if (GetInput()->IsDown(InputCode::Key_S)) moveInput.z -= 1.0f;
				if (GetInput()->IsDown(InputCode::Key_D)) moveInput.x += 1.0f;
				if (GetInput()->IsDown(InputCode::Key_A)) moveInput.x -= 1.0f;
				if (GetInput()->IsDown(InputCode::Key_E) || GetInput()->IsDown(InputCode::Key_Space)) moveInput.y += 1.0f;
				if (GetInput()->IsDown(InputCode::Key_Q) || GetInput()->IsDown(InputCode::Key_Control)) moveInput.y -= 1.0f;

				m_Camera->SetMovementInput(moveInput);
				m_Camera->isSprinting = GetInput()->IsDown(InputCode::Key_Shift);
			}
			else
			{
				m_CameraControlActive = false;
			}

			// Update camera
			m_Camera->Update(deltaTime);

			// Pass to renderer
			GetRenderer()->SetViewMatrix(m_Camera->GetViewMatrix());
			GetRenderer()->SetProjectionMatrix(m_Camera->GetProjectionMatrix());

			if (m_TestCube)
			{
				// Rotate the cube
				m_Rotation += deltaTime * m_RotationSpeed;
				glm::mat4 transform = glm::rotate(glm::mat4(1.0f), m_Rotation, glm::vec3(0, 1, 0));
				m_TestCube->SetTransform(transform);
				m_TestCube->Update(deltaTime);

				float baseRotationX = glm::radians(90.f);
				m_ToyCar->SetRotation(glm::vec3(baseRotationX, m_Rotation, 0));
			}

			// Update scene
			if (m_Scene)
				m_Scene->Update(deltaTime);

			// Handle editor shortcuts
			HandleEditorShortcuts();
		}

		void OnRender() override
		{
			DrawList drawList;

			//// Add test cube
			//if (m_TestCube)
			//{
			//	drawList.AddDrawable(m_TestCube.get());
			//}
			//
			//if (m_TestCube2)
			//{
			//	drawList.AddDrawable(m_TestCube2.get());
			//}

			if (m_ToyCarDrawable)
			{
				drawList.AddDrawable(m_ToyCarDrawable.get());
			}

			// Add scene objects
			if (m_Scene)
			{
				m_Scene->BuildDrawList(drawList);
			}

			// Sort by pipeline for efficiency
			drawList.SortByPipeline();

			// Submit to renderer
			GetRenderer()->SubmitDrawList(drawList);

			// Render editor UI
			RenderEditorUI();
		}

	private:
		// For live shader testing
		float m_Rotation = 0.0f;
		float m_RotationSpeed = 1.0f;  // Speed of rotation for live shader test
		std::unique_ptr<MeshDrawable> m_TestCube;
		std::unique_ptr<MeshDrawable> m_TestCube2;

		std::unique_ptr<Model> m_ToyCar;
		std::unique_ptr<ModelDrawable> m_ToyCarDrawable;

		std::unique_ptr<Camera> m_Camera;
		bool m_CameraControlActive = false;  // Only move camera when right-click held

		// Project state
		std::string m_CurrentProjectName = "Sandbox";
		std::filesystem::path m_CurrentProjectPath;

		// Editor state
		bool m_ShowDemoWindow = false;
		bool m_ShowMetricsWindow = false;
		bool m_ShowShaderNodeEditor = false;
		bool m_ShowAssetBrowser = false;
		bool m_ShowSceneHierarchy = true;
		bool m_ShowInspector = true;
		bool m_ShowConsole = true;
		bool m_ShowProjectSettings = false;
		bool m_ShowLiveShaderTest = true;

		// Tools
		std::unique_ptr<ShaderNodeEditor> m_ShaderNodeEditor;

		// Stats
		float m_FrameTime = 0.0f;
		int m_FrameCount = 0;
		float m_FPS = 0.0f;

		// Viewport state
		bool m_IsPlayMode = false;

		// Shader compilation state
		bool m_CompileSuccess = false;
		bool m_CompileError = false;
		std::string m_LastError;

		void SetupDefaultProject()
		{
			// First, let's understand exactly where we are
			std::filesystem::path currentPath = std::filesystem::current_path();
			std::filesystem::path exePath = std::filesystem::canonical(currentPath);

			LOG_INFO("=== PATH DEBUGGING ===");
			LOG_INFO("Current working directory: {}", currentPath.string());
			LOG_INFO("Canonical path: {}", exePath.string());

			// Let's trace up the path and see what exists at each level
			std::filesystem::path tracePath = exePath;
			int level = 0;
			while (tracePath.has_parent_path() && level < 10) {
				LOG_INFO("Level {}: {}", level, tracePath.string());

				// Check what exists at this level
				if (std::filesystem::exists(tracePath / "Sandbox")) {
					LOG_INFO("  -> Found Sandbox here!");
				}
				if (std::filesystem::exists(tracePath / "Editor")) {
					LOG_INFO("  -> Found Editor here!");
				}
				if (std::filesystem::exists(tracePath / "NightBloom")) {
					LOG_INFO("  -> Found NightBloom here!");
				}

				tracePath = tracePath.parent_path();
				level++;
			}
			LOG_INFO("=== END PATH DEBUGGING ===");

			// Now find the NightBloom_Engine root by looking for key markers
			std::filesystem::path engineRoot;
			std::filesystem::path searchPath = exePath;

			// Keep going up until we find a directory that contains BOTH Editor and Sandbox folders
			while (searchPath.has_parent_path()) {
				if (std::filesystem::exists(searchPath / "Editor") &&
					std::filesystem::exists(searchPath / "Sandbox") &&
					std::filesystem::exists(searchPath / "NightBloom")) {
					engineRoot = searchPath;
					LOG_INFO("Found NightBloom_Engine root at: {}", engineRoot.string());
					break;
				}
				searchPath = searchPath.parent_path();
			}

			if (!engineRoot.empty()) {
				m_CurrentProjectPath = engineRoot / "Sandbox";
				m_CurrentProjectName = "Sandbox";
				LOG_INFO("Using Sandbox at: {}", m_CurrentProjectPath.string());
			}
			else {
				LOG_WARN("Could not find engine root, using fallback");
				m_CurrentProjectPath = std::filesystem::path("D:/GitLibrary/Personal/NightBloom_Engine/Sandbox");
				m_CurrentProjectName = "Sandbox";
			}

			// Set up the project context for EditorFileUtils
			Editor::EditorFileUtils::ProjectContext ctx;
			ctx.root = m_CurrentProjectPath;

#ifdef _DEBUG
			ctx.config = "Debug";
#else
			ctx.config = "Release";
#endif

			Editor::EditorFileUtils::SetProjectContext(ctx);

			LOG_INFO("Current project: {} at {}", m_CurrentProjectName, m_CurrentProjectPath.string());
		}

		void UpdateWindowTitle()
		{
			std::string title = "Nightbloom Editor v0.1.0";
			title += " | Project: " + m_CurrentProjectName;

			//if (m_FPS > 0)
			//{
			//	title += " | FPS: " + std::to_string(static_cast<int>(m_FPS));
			//}

			GetWindow()->SetTitle(title);
		}

		void TestGLTFLoader()
		{
			LOG_INFO("=== Testing GLTF Loader ===");

			GLTFLoader loader;

			// Try to load a test model
			// You'll need to put a .gltf or .glb file in your Models folder
			std::string modelPath = AssetManager::Get().GetModelPath("ToyCar/ToyCar.gltf");

			auto modelData = loader.Load(modelPath);

			if (!modelData)
			{
				LOG_ERROR("GLTF Test FAILED: {}", loader.GetLastError());
				return;
			}

			// Log what we loaded
			LOG_INFO("GLTF Test SUCCESS!");
			LOG_INFO("  Model: {}", modelData->name);
			LOG_INFO("  Meshes: {}", modelData->meshes.size());
			LOG_INFO("  Materials: {}", modelData->materials.size());
			LOG_INFO("  Total Vertices: {}", modelData->totalVertices);
			LOG_INFO("  Total Indices: {}", modelData->totalIndices);

			// Log each mesh
			for (size_t i = 0; i < modelData->meshes.size(); ++i)
			{
				const auto& mesh = modelData->meshes[i];
				LOG_INFO("  Mesh[{}] '{}': {} verts, {} indices, bounds: ({:.2f},{:.2f},{:.2f}) to ({:.2f},{:.2f},{:.2f})",
					i, mesh.name,
					mesh.vertices.size(), mesh.indices.size(),
					mesh.boundsMin.x, mesh.boundsMin.y, mesh.boundsMin.z,
					mesh.boundsMax.x, mesh.boundsMax.y, mesh.boundsMax.z);

				// Log first few vertices to verify data looks correct
				if (!mesh.vertices.empty())
				{
					const auto& v = mesh.vertices[0];
					LOG_INFO("    First vertex: pos=({:.3f},{:.3f},{:.3f}), normal=({:.3f},{:.3f},{:.3f}), uv=({:.3f},{:.3f})",
						v.position.x, v.position.y, v.position.z,
						v.normal.x, v.normal.y, v.normal.z,
						v.texCoord.x, v.texCoord.y);
				}
			}

			// Log materials
			for (size_t i = 0; i < modelData->materials.size(); ++i)
			{
				const auto& mat = modelData->materials[i];
				LOG_INFO("  Material[{}] '{}': color=({:.2f},{:.2f},{:.2f},{:.2f}), metallic={:.2f}, roughness={:.2f}",
					i, mat.name,
					mat.baseColorFactor.r, mat.baseColorFactor.g, mat.baseColorFactor.b, mat.baseColorFactor.a,
					mat.metallicFactor, mat.roughnessFactor);

				if (!mat.baseColorTexturePath.empty())
					LOG_INFO("    Albedo texture: {}", mat.baseColorTexturePath);
			}

			LOG_INFO("=== GLTF Loader Test Complete ===");
		}

		void LoadToyCar()
		{
			LOG_INFO("=== Loading ToyCar Model ===");

			m_ToyCar = std::make_unique<Model>("ToyCar");

			std::string modelPath = AssetManager::Get().GetModelPath("ToyCar/ToyCar.gltf");

			if (!m_ToyCar->LoadFromFile(modelPath,
				GetRenderer()->GetResourceManager(),
				GetRenderer()->GetDescriptorManager()))
			{
				LOG_ERROR("Failed to load ToyCar model");
				m_ToyCar.reset();
				return;
			}

			// The model is HUGE (raw verts are ~350 units) but has a 0.0001 scale in glTF
			// Apply that scale here
			m_ToyCar->SetScale(0.01f);  // Adjust as needed to fit your scene
			m_ToyCar->SetPosition(glm::vec3(-3.0f, -2.0f, -3.0f));
			
			ResourceManager* resources = GetRenderer()->GetResourceManager();
			Texture* defaultTex = resources ? resources->GetTexture("default_white") : nullptr;

			// Create drawable
			m_ToyCarDrawable = std::make_unique<ModelDrawable>(m_ToyCar.get(), defaultTex);

			LOG_INFO("=== ToyCar Model Loaded ===");
		}

		void RenderEditorUI()
		{
			// Main menu bar
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("New Project...")) { NewProject(); }
					if (ImGui::MenuItem("Open Project...")) { OpenProject(); }
					if (ImGui::MenuItem("Save Project", "Ctrl+S")) { SaveProject(); }
					ImGui::Separator();
					if (ImGui::MenuItem("New Scene", "Ctrl+N")) { NewScene(); }
					if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) { OpenScene(); }
					if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { SaveScene(); }
					if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) { SaveSceneAs(); }
					ImGui::Separator();
					if (ImGui::MenuItem("Project Settings...")) { m_ShowProjectSettings = true; }
					ImGui::Separator();
					if (ImGui::MenuItem("Exit", "Alt+F4")) { Quit(); }
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Edit"))
				{
					if (ImGui::MenuItem("Play", "F5")) { TogglePlayMode(); }
					if (ImGui::MenuItem("Stop", "F7", false, m_IsPlayMode)) { StopPlayMode(); }
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Tools"))
				{
					if (ImGui::MenuItem("Shader Editor", nullptr, &m_ShowShaderNodeEditor)) {}
					if (ImGui::MenuItem("Asset Browser", nullptr, &m_ShowAssetBrowser)) {}
					ImGui::Separator();
					ImGui::Separator();
					if (ImGui::MenuItem("Compile All Shaders")) { CompileAllShaders(); }
					if (ImGui::MenuItem("Reload Shaders", "Ctrl+R"))
					{
						if (GetRenderer()) GetRenderer()->ReloadShaders();
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("View"))
				{
					if (ImGui::MenuItem("Scene Hierarchy", nullptr, &m_ShowSceneHierarchy)) {}
					if (ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector)) {}
					if (ImGui::MenuItem("Console", nullptr, &m_ShowConsole)) {}
					ImGui::Separator();
					if (ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow)) {}
					if (ImGui::MenuItem("ImGui Metrics", nullptr, &m_ShowMetricsWindow)) {}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Help"))
				{
					if (ImGui::MenuItem("Documentation")) { OpenDocumentation(); }
					if (ImGui::MenuItem("About")) { ShowAboutDialog(); }
					ImGui::EndMenu();
				}

				// Right side of menu bar - play controls
				ImGui::Separator();
				float menuWidth = ImGui::GetWindowWidth();
				if (menuWidth > 400) {
					ImGui::SameLine(menuWidth - 350);
					ImGui::Text("Project: %s", m_CurrentProjectName.c_str());

					ImGui::SameLine(menuWidth - 200);

					if (!m_IsPlayMode)
					{
						if (ImGui::Button("Play")) { TogglePlayMode(); }
					}
					else
					{
						if (ImGui::Button("Stop")) { StopPlayMode(); }
					}
				}

				ImGui::EndMainMenuBar();
			}

#if USE_IMGUI_DOCKING
			// Dockspace - only if docking is available
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);

			ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

			ImGui::Begin("DockSpace", nullptr, windowFlags);
			ImGui::PopStyleVar(3);

			ImGuiID dockspaceId = ImGui::GetID("EditorDockSpace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

			ImGui::End();
#endif

			// Editor panels (these work with or without docking)
			if (m_ShowSceneHierarchy) RenderSceneHierarchy();
			if (m_ShowInspector) RenderInspector();
			if (m_ShowConsole) RenderConsole();
			if (m_ShowAssetBrowser) RenderAssetBrowser();
			if (m_ShowProjectSettings) RenderProjectSettings();

			if (m_ShowShaderNodeEditor && m_ShaderNodeEditor)
			{
				m_ShaderNodeEditor->Draw("Shader Node Editor", &m_ShowShaderNodeEditor);

				RenderShaderCompiler();
			}

			if (m_ShowLiveShaderTest) RenderLiveShaderTest();

			// Always render viewport
			RenderSceneViewport();

			// Demo windows
			if (m_ShowDemoWindow) ImGui::ShowDemoWindow(&m_ShowDemoWindow);
			if (m_ShowMetricsWindow) ImGui::ShowMetricsWindow(&m_ShowMetricsWindow);
		}

		void RenderShaderCompiler()
		{
			ImGui::Begin("Shader Compiler");

			ImGui::Text("Node Graph Shader Compiler");
			ImGui::Text("Target Project: %s", m_CurrentProjectName.c_str());
			ImGui::Separator();

			if (ImGui::Button("Compile Shader", ImVec2(150, 30)))
			{
				if (m_ShaderNodeEditor && m_ShaderNodeEditor->CompileShaders())
				{
					std::string vertShader = m_ShaderNodeEditor->GetVertexShader();
					std::string fragShader = m_ShaderNodeEditor->GetFragmentShader();

					// Save and compile shaders
					bool vertSuccess = Editor::EditorFileUtils::SaveShaderFile("NodeGenerated.vert", vertShader);
					bool fragSuccess = Editor::EditorFileUtils::SaveShaderFile("NodeGenerated.frag", fragShader);

					if (vertSuccess && fragSuccess)
					{
						LOG_INFO("Shaders compiled and deployed to project successfully!");
						m_CompileSuccess = true;
						m_CompileError = false;
						m_LastError.clear();

						// Create or reload the NodeGenerated pipeline
						PipelineConfig config;
						config.vertexShaderPath = "NodeGenerated.vert";
						config.fragmentShaderPath = "NodeGenerated.frag";
						config.useVertexInput = true;  // or false depending on your needs

						// Use generic enums
						config.topology = Nightbloom::PrimitiveTopology::TriangleList;
						config.polygonMode = Nightbloom::PolygonMode::Fill;
						config.cullMode = Nightbloom::CullMode::Back;
						config.frontFace = Nightbloom::FrontFace::CounterClockwise;
						config.depthTestEnable = true;
						config.depthWriteEnable = true;
						config.depthCompareOp = Nightbloom::CompareOp::Less;
						config.blendEnable = false;
						config.useUniformBuffer = true;  // ADD THIS
						config.useTextures = m_ShaderNodeEditor->UsesTextures();

						//Push constants if needed
						config.pushConstantSize = sizeof(PushConstantData);
						config.pushConstantStages = Nightbloom::ShaderStage::VertexFragment;

						// Get the pipeline manager from renderer
						if (GetRenderer()->GetPipelineManager()->CreatePipeline(
							Nightbloom::PipelineType::NodeGenerated,config))
						{
							// AUTOMATICALLY SWITCH TO THE NEW SHADER
							m_CurrentTestPipeline = PipelineType::NodeGenerated;

							// Update test cube
							if (m_TestCube)
							{
								auto* vb = GetRenderer()->GetTestVertexBuffer();
								auto* ib = GetRenderer()->GetTestIndexBuffer();
								uint32_t ic = GetRenderer()->GetTestIndexCount();

								if (vb && ib && ic > 0)
								{
									m_TestCube = std::make_unique<MeshDrawable>(
										vb, ib, ic, PipelineType::NodeGenerated
									);
									//m_TestCube->SetViewMatrix(m_ViewMatrix);
									//m_TestCube->SetProjectionMatrix(m_ProjectionMatrix);

									if (auto* resources = GetRenderer()->GetResourceManager())
									{
										if (auto* checker = resources->GetTexture("uv_checker"))
										{
											m_TestCube->AddTexture(checker);
											LOG_INFO("Attached uv_checker to NodeGenerated drawable after compile");
										}
										else
										{
											LOG_WARN("uv_checker texture missing after compile");
										}
									}

									LOG_INFO("Now rendering with NodeGenerated shader!");
								}
							}
						}
					}
					else
					{
						LOG_ERROR("Failed to compile/deploy shaders");
						m_CompileSuccess = false;
						m_CompileError = true;
						m_LastError = "Failed to compile or deploy shaders to project";
					}
				}
				else
				{
					LOG_ERROR("Shader generation failed!");
					m_CompileSuccess = false;
					m_CompileError = true;
					if (m_ShaderNodeEditor)
					{
						m_LastError = m_ShaderNodeEditor->GetLastError();
					}
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Apply to Scene", ImVec2(150, 30)))
			{
				if (GetRenderer())
				{
					GetRenderer()->ReloadShaders();
					LOG_INFO("Reloaded shaders in renderer");
				}
			}

			// Show compile status
			if (m_CompileSuccess)
			{
				ImGui::TextColored(ImVec4(0, 1, 0, 1), "Compilation successful!");
				ImGui::Text("Shaders deployed to: %s/Shaders/", m_CurrentProjectName.c_str());
			}
			else if (m_CompileError)
			{
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Compilation failed!");
				if (!m_LastError.empty())
				{
					ImGui::TextWrapped("Error: %s", m_LastError.c_str());
				}
			}

			ImGui::Separator();
			ImGui::Text("Instructions:");
			ImGui::BulletText("Right-click canvas to add nodes");
			ImGui::BulletText("Drag from output to input pins");
			ImGui::BulletText("Middle mouse to pan");
			ImGui::BulletText("Select node + Delete to remove");
			ImGui::BulletText("Double-click node for properties");

			ImGui::End();
		}

		// ADD THIS NEW METHOD for live shader testing
		void RenderLiveShaderTest()
		{
			ImGui::Begin("Live Shader Test", &m_ShowLiveShaderTest);

			// Pipeline selector
			const char* pipelineNames[] = {
				"Triangle", "Mesh", "Shadow", "Skybox",
				"Volumetric", "PostProcess", "Compute", "NodeGenerated"
			};

			int currentPipeline = static_cast<int>(m_CurrentTestPipeline);
			if (ImGui::Combo("Test Pipeline", &currentPipeline, pipelineNames,
				static_cast<int>(PipelineType::Count)))
			{
				m_CurrentTestPipeline = static_cast<PipelineType>(currentPipeline);

				// Update the test cube to use new pipeline
				if (m_TestCube)
				{
					auto* pipelineManager = GetRenderer()->GetPipelineManager();
					if (pipelineManager->GetPipeline(m_CurrentTestPipeline) == nullptr)
					{
						LOG_ERROR("Pipeline {} does not exist!", pipelineNames[static_cast<int>(m_CurrentTestPipeline)]);
					//	return;
					}

					auto* vb = GetRenderer()->GetTestVertexBuffer();
					auto* ib = GetRenderer()->GetTestIndexBuffer();
					uint32_t ic = GetRenderer()->GetTestIndexCount();

					if (vb && ib && ic > 0)
					{
						m_TestCube = std::make_unique<MeshDrawable>(
							vb, ib, ic, m_CurrentTestPipeline
						);
						//m_TestCube->SetViewMatrix(m_ViewMatrix);
						//m_TestCube->SetProjectionMatrix(m_ProjectionMatrix);

						ResourceManager* resources = GetRenderer()->GetResourceManager();
						if (resources)
						{
							VulkanTexture* texture = resources->GetTexture("uv_checker");
							if (texture)
							{
								m_TestCube->AddTexture(texture);
							}
						}

						LOG_INFO("Switched to {} pipeline", pipelineNames[currentPipeline]);
					}
				}
			}

			ImGui::Separator();
			ImGui::Text("The cube is now using: %s", pipelineNames[currentPipeline]);

			static int textureChoice = 0;
			const char* textureNames[] = { "UV Checker", "White", "Black", "Normal" };

			if (ImGui::Combo("Texture", &textureChoice, textureNames, 4))
			{
				ResourceManager* resources = GetRenderer()->GetResourceManager();
				if (resources && m_TestCube)
				{
					m_TestCube->ClearTextures();

					const char* textureLookup[] = { "uv_checker", "default_white", "default_black", "default_normal" };
					VulkanTexture* texture = resources->GetTexture(textureLookup[textureChoice]);

					if (texture)
					{
						m_TestCube->AddTexture(texture);
						LOG_INFO("Switched to {} texture", textureNames[textureChoice]);
					}
				}
			}

			// Rotation speed control
			ImGui::SliderFloat("Rotation Speed", &m_RotationSpeed, 0.0f, 5.0f);

			if (ImGui::Button("Reset Rotation"))
			{
				m_Rotation = 0.0f;
				m_RotationSpeed = 1.0f;
			}

			ImGui::End();
		}

		void RenderProjectSettings()
		{
			ImGui::Begin("Project Settings", &m_ShowProjectSettings);

			ImGui::Text("Current Project: %s", m_CurrentProjectName.c_str());
			ImGui::Text("Project Path: %s", m_CurrentProjectPath.string().c_str());

			ImGui::Separator();

			// Build configuration
			static int buildConfig = 0;
			const char* configs[] = { "Debug", "Release", "RelWithDebInfo" };
			if (ImGui::Combo("Build Configuration", &buildConfig, configs, 3))
			{
				Editor::EditorFileUtils::ProjectContext ctx = Editor::EditorFileUtils::GetProjectContext();
				ctx.config = configs[buildConfig];
				Editor::EditorFileUtils::SetProjectContext(ctx);
				LOG_INFO("Changed build configuration to: {}", configs[buildConfig]);
			}

			ImGui::Separator();

			// Shader settings
			ImGui::Text("Shader Settings");
			ImGui::Text("Source: %s", Editor::EditorFileUtils::GetEditorShadersSourceDirectory().c_str());
			ImGui::Text("Compiled: %s", Editor::EditorFileUtils::GetEditorShadersCompiledDirectory().c_str());
			ImGui::Text("Deploy to: %s", Editor::EditorFileUtils::GetCurrentProjectShadersDirectory().c_str());

			ImGui::End();
		}

		void RenderSceneHierarchy()
		{
			ImGui::Begin("Scene Hierarchy", &m_ShowSceneHierarchy);

			// Scene tree
			if (ImGui::TreeNode("Scene Root"))
			{
				if (ImGui::TreeNode("Camera"))
				{
					ImGui::Text("Main Camera");
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Cube"))
				{
					ImGui::Text("Test Cube");
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Lights"))
				{
					ImGui::Text("Directional Light");
					ImGui::TreePop();
				}

				ImGui::TreePop();
			}

			ImGui::End();
		}

		void RenderInspector()
		{
			ImGui::Begin("Inspector", &m_ShowInspector);

			ImGui::Text("Selected: Cube");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
			{
				static float position[3] = { 0.0f, 0.0f, 0.0f };
				static float rotation[3] = { 0.0f, 0.0f, 0.0f };
				static float scale[3] = { 1.0f, 1.0f, 1.0f };

				ImGui::DragFloat3("Position", position, 0.1f);
				ImGui::DragFloat3("Rotation", rotation, 1.0f);
				ImGui::DragFloat3("Scale", scale, 0.1f);
			}

			if (ImGui::CollapsingHeader("Mesh"))
			{
				ImGui::Text("Mesh: Cube");
				ImGui::Text("Vertices: 8");
				ImGui::Text("Triangles: 12");
			}

			if (ImGui::CollapsingHeader("Material"))
			{
				ImGui::Text("Shader: Default");
				static float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
				ImGui::ColorEdit4("Color", color);
			}

			ImGui::End();
		}

		void RenderConsole()
		{
			ImGui::Begin("Console", &m_ShowConsole);

			ImGui::Text("Console output:");
			ImGui::Separator();

			// Simple log display
			ImGui::BeginChild("LogScroll", ImVec2(0, -25), true);
			ImGui::Text("[INFO] Editor started");
			ImGui::Text("[INFO] Renderer initialized");
			ImGui::EndChild();

			// Command input
			static char inputBuf[256] = "";
			if (ImGui::InputText("Command", inputBuf, sizeof(inputBuf),
				ImGuiInputTextFlags_EnterReturnsTrue))
			{
				LOG_INFO("Console command: {}", inputBuf);
				inputBuf[0] = '\0';
			}

			ImGui::End();
		}

		void RenderAssetBrowser()
		{
			ImGui::Begin("Asset Browser", &m_ShowAssetBrowser);

			if (ImGui::Button("Import Asset..."))
			{
				// Open file dialog
			}

			ImGui::Separator();

			// Asset tree
			if (ImGui::TreeNode("Shaders"))
			{
				ImGui::Text("triangle.vert");
				ImGui::Text("triangle.frag");
				ImGui::Text("mesh.vert");
				ImGui::Text("mesh.frag");
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Textures"))
			{
				ImGui::Text("No textures");
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Models"))
			{
				ImGui::Text("No models");
				ImGui::TreePop();
			}

			ImGui::End();
		}

		void RenderSceneViewport()
		{
			ImGui::Begin("Viewport");

			// Viewport controls
			if (ImGui::Button("Perspective")) {}
			ImGui::SameLine();
			if (ImGui::Button("Top")) {}
			ImGui::SameLine();
			if (ImGui::Button("Side")) {}

			ImGui::Separator();

			// Add pipeline toggle button
			if (ImGui::Button("Toggle Pipeline (P)")) {
				if (GetRenderer()) GetRenderer()->TogglePipeline();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload Shaders (R)")) {
				if (GetRenderer()) GetRenderer()->ReloadShaders();
			}

			ImGui::Separator();

			// Viewport info
			ImVec2 viewportSize = ImGui::GetContentRegionAvail();
			ImGui::Text("3D Viewport (%dx%d)", (int)viewportSize.x, (int)viewportSize.y);
			ImGui::Text("Current Project: %s", m_CurrentProjectName.c_str());
			
			if (m_IsPlayMode)
			{
				ImGui::TextColored(ImVec4(0, 1, 0, 1), "PLAYING");
			}

			ImGui::End();
		}

		void HandleEditorShortcuts()
		{
			if (!GetInput()) return;

			bool ctrl = GetInput()->IsDown(InputCode::Key_Control);
				//|| GetInput()->IsDown(InputCode::Key_RightControl);

			// File shortcuts
			if (ctrl && GetInput()->IsPressed(InputCode::Key_N)) NewScene();
			if (ctrl && GetInput()->IsPressed(InputCode::Key_O)) OpenScene();
			if (ctrl && GetInput()->IsPressed(InputCode::Key_S)) SaveScene();

			// Play mode
			if (GetInput()->IsPressed(InputCode::Key_F5)) TogglePlayMode();
			if (GetInput()->IsPressed(InputCode::Key_F7)) StopPlayMode();

			// Tools
			if (ctrl && GetInput()->IsPressed(InputCode::Key_R))
			{
				if (GetRenderer()) GetRenderer()->ReloadShaders();
			}

			// Pipeline toggle
			if (GetInput()->IsPressed(InputCode::Key_P))
			{
				if (GetRenderer()) GetRenderer()->TogglePipeline();
			}

			// Exit
			if (GetInput()->IsPressed(InputCode::Key_Escape))
			{
				LOG_INFO("Escape pressed - exiting editor");
				Quit();
			}
		}

		// Editor operations
		// Project management
		void NewProject()
		{
			LOG_INFO("Creating new project");
			// TODO: Project creation wizard
		}

		void OpenProject()
		{
			LOG_INFO("Opening project");
			// TODO: Project browser dialog
		}

		void SaveProject()
		{
			LOG_INFO("Saving project: {}", m_CurrentProjectName);
			// TODO: Save project settings
		}

		// Scene management
		void NewScene()
		{
			LOG_INFO("Creating new scene");
		}

		void OpenScene()
		{
			LOG_INFO("Opening scene");
		}

		void SaveScene()
		{
			LOG_INFO("Saving scene");
		}

		void SaveSceneAs()
		{
			LOG_INFO("Save scene as");
		}

		void TogglePlayMode()
		{
			m_IsPlayMode = !m_IsPlayMode;
			LOG_INFO(m_IsPlayMode ? "Entering play mode" : "Exiting play mode");
		}

		void StopPlayMode()
		{
			if (m_IsPlayMode)
			{
				m_IsPlayMode = false;
				LOG_INFO("Stopping play mode");
			}
		}

		void CompileAllShaders()
		{
			LOG_INFO("Compiling all shaders in project");
			// TODO: Batch compile all shaders
		}

		bool CompileAndApplyNodeShaders()
		{
			if (!m_ShaderNodeEditor)
			{
				LOG_ERROR("Shader node editor not initialized");
				m_CompileError = true;
				m_LastError = "Shader node editor not initialized";
				return false;
			}

			// Generate shader code from the node graph
			if (!m_ShaderNodeEditor->CompileShaders())
			{
				LOG_ERROR("Failed to generate shader code from nodes");
				m_CompileSuccess = false;
				m_CompileError = true;
				m_LastError = m_ShaderNodeEditor->GetLastError();
				return false;
			}

			std::string vertCode = m_ShaderNodeEditor->GetVertexShader();
			std::string fragCode = m_ShaderNodeEditor->GetFragmentShader();

			// Save and compile GLSL to SPIR-V
			bool vertSuccess = Editor::EditorFileUtils::SaveShaderFile("NodeGenerated.vert", vertCode);
			bool fragSuccess = Editor::EditorFileUtils::SaveShaderFile("NodeGenerated.frag", fragCode);

			if (!vertSuccess || !fragSuccess)
			{
				LOG_ERROR("Failed to compile shaders to SPIR-V");
				m_CompileSuccess = false;
				m_CompileError = true;
				m_LastError = "Failed to compile shaders to SPIR-V";
				return false;
			}

			// Get the resource manager from renderer
			ResourceManager* resources = GetRenderer()->GetResourceManager();
			if (!resources)
			{
				LOG_ERROR("Resource manager not available");
				m_CompileSuccess = false;
				m_CompileError = true;
				m_LastError = "Resource manager not available";
				return false;
			}

			// Remove old node shaders if they exist
			resources->DestroyShader("node_vert");
			resources->DestroyShader("node_frag");

			// Load the newly compiled shaders
			VulkanShader* vertShader = resources->LoadShader(
				"node_vert",
				ShaderStage::Vertex,
				"NodeGenerated.vert.spv"
			);

			VulkanShader* fragShader = resources->LoadShader(
				"node_frag",
				ShaderStage::Fragment,
				"NodeGenerated.frag.spv"
			);

			if (!vertShader || !fragShader)
			{
				LOG_ERROR("Failed to load compiled node shaders");
				m_CompileSuccess = false;
				m_CompileError = true;
				m_LastError = "Failed to load compiled shaders into resource manager";
				return false;
			}

			// Now create the pipeline using the loaded shaders
			// We need to get the Vulkan pipeline manager directly
			//VulkanPipelineAdapter* adapter = static_cast<VulkanPipelineAdapter*>(
			//	GetRenderer()->GetPipelineManager()
			//	);
			//VulkanPipelineManager* vkPipelineManager = adapter->GetVulkanManager();

			// Create pipeline config with shader objects
	// Create the NodeGenerated pipeline
			//VulkanPipelineAdapter* adapter = static_cast<VulkanPipelineAdapter*>(
			//	GetRenderer()->GetPipelineManager()
			//	);
			//VulkanPipelineManager* vkPipelineManager = adapter->GetVulkanManager();

			PipelineConfig config;
			config.vertexShader = vertShader;
			config.fragmentShader = fragShader;
			config.useVertexInput = true;
			config.topology = PrimitiveTopology::TriangleList;
			config.polygonMode = PolygonMode::Fill;
			config.cullMode = CullMode::Back;
			config.frontFace = FrontFace::CounterClockwise;
			config.depthTestEnable = true;
			config.depthWriteEnable = true;
			config.depthCompareOp = CompareOp::Less;
			config.useTextures = m_ShaderNodeEditor->UsesTextures();
			config.useUniformBuffer = true;
			config.pushConstantSize = sizeof(PushConstantData);
			config.pushConstantStages = ShaderStage::VertexFragment;

			if (GetRenderer()->GetPipelineManager()->CreatePipeline(PipelineType::NodeGenerated, config))
			{
				LOG_INFO("Successfully created NodeGenerated pipeline");

				// Switch to the new pipeline
				m_CurrentTestPipeline = PipelineType::NodeGenerated;
				//UpdateTestCube();  // Your existing method to update the cube

				return true;
			}

			LOG_ERROR("Failed to create NodeGenerated pipeline");
			return false;
		}

		// Help
		void OpenDocumentation()
		{
			LOG_INFO("Opening documentation");
			// TODO: Open web browser to docs
		}

		void ShowAboutDialog()
		{
			LOG_INFO("Nightbloom Editor v0.1.0");
		}

		void LoadEditorSettings()
		{
			// TODO: Load from config file
		}

		void SaveEditorSettings()
		{
			// TODO: Save to config file
		}
	};

} // namespace Nightbloom

// Entry point
Nightbloom::Application* Nightbloom::CreateApplication()
{
	return new Nightbloom::EditorApplication();
}