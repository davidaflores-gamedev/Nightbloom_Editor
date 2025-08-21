//------------------------------------------------------------------------------
// EditorApp.cpp
//
// Nightbloom Editor Application - Fixed for ImGui compatibility
// Copyright (c) 2024. All rights reserved.
//------------------------------------------------------------------------------

#include "Engine/Core/Application.hpp"
#include "Engine/Core/Logger/Logger.hpp"
#include <imgui.h>
#include <memory>
#include <fstream>

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

			// Enable docking if available

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
				GetWindow()->SetTitle("Nightbloom Editor v0.1.0");
				//GetWindow()->Maximize();
			}

			#if USE_IMGUI_DOCKING
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
			#endif
		}

		void OnUpdate(float deltaTime) override
		{
			//ImGui_ImplVulkan_NewFrame();
			//ImGui_ImplWin32_NewFrame();
			//ImGui::NewFrame();

			// Update editor systems
			m_FrameTime += deltaTime;
			m_FrameCount++;

			// Update title bar with stats every second
			if (m_FrameTime >= 1.0f)
			{
				float fps = m_FrameCount / m_FrameTime;
				std::string title = "Nightbloom Editor - FPS: " + std::to_string(static_cast<int>(fps));
				GetWindow()->SetTitle(title);
				m_FrameTime = 0.0f;
				m_FrameCount = 0;
			}

			// Handle editor shortcuts
			HandleEditorShortcuts();
		}

		void OnRender() override
		{
			// Render editor UI
			RenderEditorUI();
		}

	private:
		// Editor state
		bool m_ShowDemoWindow = false;
		bool m_ShowMetricsWindow = false;
		bool m_ShowShaderEditor = false;
		bool m_ShowAssetBrowser = false;
		bool m_ShowSceneHierarchy = true;
		bool m_ShowInspector = true;
		bool m_ShowConsole = true;

		// Stats
		float m_FrameTime = 0.0f;
		int m_FrameCount = 0;

		// Viewport state
		bool m_IsPlayMode = false;

		void RenderEditorUI()
		{
			// Main menu bar
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("New Scene", "Ctrl+N")) { NewScene(); }
					if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) { OpenScene(); }
					if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { SaveScene(); }
					if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) { SaveSceneAs(); }
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
					if (ImGui::MenuItem("Shader Editor", nullptr, &m_ShowShaderEditor)) {}
					if (ImGui::MenuItem("Asset Browser", nullptr, &m_ShowAssetBrowser)) {}
					ImGui::Separator();
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
					if (ImGui::MenuItem("About")) { ShowAboutDialog(); }
					ImGui::EndMenu();
				}

				// Right side of menu bar - play controls
				ImGui::Separator();
				float menuWidth = ImGui::GetWindowWidth();
				if (menuWidth > 200) {
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
			if (m_ShowShaderEditor) RenderShaderEditor();

			// Always render viewport
			RenderSceneViewport();

			// Demo windows
			if (m_ShowDemoWindow) ImGui::ShowDemoWindow(&m_ShowDemoWindow);
			if (m_ShowMetricsWindow) ImGui::ShowMetricsWindow(&m_ShowMetricsWindow);
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

			// The actual 3D viewport will be rendered here
			ImVec2 viewportSize = ImGui::GetContentRegionAvail();
			ImGui::Text("3D Viewport (%dx%d)", (int)viewportSize.x, (int)viewportSize.y);
			ImGui::Text("Press P to toggle pipeline");
			ImGui::Text("Press R to reload shaders");

			ImGui::End();
		}

		void RenderShaderEditor()
		{
			ImGui::Begin("Shader Editor", &m_ShowShaderEditor);

			ImGui::Text("Shader Node Editor");
			ImGui::Separator();

			if (ImGui::Button("Create Color Node"))
			{
				LOG_INFO("Creating color node");
			}

			ImGui::SameLine();

			if (ImGui::Button("Create Texture Node"))
			{
				LOG_INFO("Creating texture node");
			}

			ImGui::Separator();
			ImGui::Text("Node graph will appear here");
			ImGui::Text("(Full implementation in Tools/ShaderEditor/)");

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