#pragma once

#include <imgui.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace Nightbloom
{

	// Forward declarations
	class ShaderNode;
	class ShaderGraph;

	// Pin types for node connections
	enum class PinType
	{
		Float,
		Vec2,
		Vec3,
		Vec4,
		Texture2D,
		Matrix4
	};

	// Visual style for pins
	struct PinStyle {
		ImU32 color;
		float radius = 5.0f;
	};

	// Node pin (input/outpu)
	struct NodePin
	{
		std::string name;
		PinType type;
		bool isInput;
		int nodeId;
		int pinId;

		// For output pins - can connect to multiple inputs
		std::vector<int> connections;

		// For input pins - single connection
		int connectedTo = -1;

		ImVec2 pos; // Screen position for rendering
	};

	// Connection between two pins
	struct NodeConnection {
		int id;
		int startNode;
		int startPin;
		int endNode;
		int endPin;
	};

	// Base class for shader nodes
	class ShaderNode {
	public:
		ShaderNode(const std::string& name, int id);
		virtual ~ShaderNode() = default;

		// Generate GLSL code for this node
		virtual std::string GenerateGLSL(const ShaderGraph* graph) const = 0;

		// Get variable name for this node's output
		virtual std::string GetOutputVariable(int outputIndex = 0) const;

		// UI
		virtual void DrawProperties() {} // Override for custom properties

		// Node data
		std::string name;
		int id;
		ImVec2 position;
		ImVec2 size;

		std::vector<NodePin> inputPins;
		std::vector<NodePin> outputPins;

	protected:
		void AddInputPin(const std::string& name, PinType type);
		void AddOutputPin(const std::string& name, PinType type);
	};

	// Concrete node types
	class TextureNode : public ShaderNode {
	public:
		TextureNode(int id);
		std::string GenerateGLSL(const ShaderGraph* graph) const override;
		void DrawProperties() override;

		int textureSlot = 0;
	};

	class ColorNode : public ShaderNode {
	public:
		ColorNode(int id);
		std::string GenerateGLSL(const ShaderGraph* graph) const override;
		void DrawProperties() override;

		float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	};

	class TimeNode : public ShaderNode {
	public:
		TimeNode(int id);
		std::string GenerateGLSL(const ShaderGraph* graph) const override;
	};

	class MultiplyNode : public ShaderNode {
	public:
		MultiplyNode(int id);
		std::string GenerateGLSL(const ShaderGraph* graph) const override;
	};

	class AddNode : public ShaderNode {
	public:
		AddNode(int id);
		std::string GenerateGLSL(const ShaderGraph* graph) const override;
	};

	class FragmentOutputNode : public ShaderNode {
	public:
		FragmentOutputNode(int id);
		std::string GenerateGLSL(const ShaderGraph* graph) const override;
	};

	// Shader graph manager
	class ShaderGraph {
	public:
		ShaderGraph();

		// Node management
		void AddNode(std::unique_ptr<ShaderNode> node);
		void RemoveNode(int nodeId);
		ShaderNode* GetNode(int nodeId);
		const ShaderNode* GetNode(int nodeId) const;

		// Connection management
		bool CanConnect(int startNode, int startPin, int endNode, int endPin) const;
		void Connect(int startNode, int startPin, int endNode, int endPin);
		void Disconnect(int connectionId);

		// Code generation
		std::string GenerateFragmentShader() const;
		std::string GenerateVertexShader() const;

		// Data
		std::vector<std::unique_ptr<ShaderNode>> nodes;
		std::vector<NodeConnection> connections;
		int nextNodeId = 1;
		int nextConnectionId = 1;
	};

	// Main editor class
	class ShaderNodeEditor {
	public:
		ShaderNodeEditor();
		~ShaderNodeEditor();

		// Main UI function - call this in your ImGui loop
		void Draw(const char* title, bool* p_open = nullptr);

		// Shader compilation
		bool CompileShaders();
		std::string GetLastError() const { return lastError; }

		// Get generated shaders
		std::string GetVertexShader() const { return vertexShaderCode; }
		std::string GetFragmentShader() const { return fragmentShaderCode; }

	private:
		// UI State
		ImVec2 scrolling = ImVec2(0.0f, 0.0f);
		float nodeSlotRadius = 5.0f;
		float gridSize = 32.0f;
		bool showGrid = true;

		// Interaction state
		int selectedNodeId = -1;
		int hoveredNodeId = -1;
		int hoveredPinId = -1;
		bool isDraggingNode = false;
		bool isDraggingCanvas = false;
		bool isDraggingConnection = false;

		// Connection dragging
		int dragConnectionStart = -1;
		int dragConnectionPin = -1;
		ImVec2 dragConnectionEnd;

		// Graph
		std::unique_ptr<ShaderGraph> graph;

		// Generated shader code
		std::string vertexShaderCode;
		std::string fragmentShaderCode;
		std::string lastError;

		// Style
		std::unordered_map<PinType, PinStyle> pinStyles;

		// Internal methods
		void DrawGrid(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
		void DrawNodes(ImDrawList* drawList, const ImVec2& canvasPos);
		void DrawConnections(ImDrawList* drawList, const ImVec2& canvasPos);
		void DrawNode(ImDrawList* drawList, ShaderNode* node, const ImVec2& offset);
		void DrawNodePin(ImDrawList* drawList, const NodePin& pin, const ImVec2& pinPos);

		void HandleNodeInteraction(ShaderNode* node, const ImVec2& nodeRectMin);
		void HandlePinInteraction(ShaderNode* node, NodePin& pin, const ImVec2& pinPos);
		void HandleCanvasInteraction(const ImVec2& canvasPos, const ImVec2& canvasSize);
		void HandleContextMenu(const ImVec2& canvasPos);

		ImVec2 GetPinPosition(const ShaderNode* node, const NodePin& pin, const ImVec2& offset) const;
		ImU32 GetPinColor(PinType type) const;

		void CreateNode(const std::string& nodeType, const ImVec2& position);
		void DeleteSelectedNode();

		// Bezier curve for connections
		void DrawBezierConnection(ImDrawList* drawList, const ImVec2& p1, const ImVec2& p2, ImU32 color, float thickness = 2.0f);
	};
}