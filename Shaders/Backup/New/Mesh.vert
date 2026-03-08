//------------------------------------------------------------------------------
// Mesh.vert
//
// Vertex shader for PBR mesh rendering with shadow mapping support
// Outputs world position, normal, texcoord, and light-space position
//------------------------------------------------------------------------------
#version 450

// Vertex input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;


// Output to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragLightSpacePos;  // NEW: Position in light space

// Frame uniforms
layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 time;
    vec4 cameraPos;
} frame;

// Light data structure (must match CPU side for std140)
struct LightData {
    vec4 position;
    vec4 color;
    vec4 attenuation;
};

// Shadow data structure
struct ShadowData {
    mat4 lightSpaceMatrix;
    vec4 shadowParams;
};

// Scene lighting UBO - needed in vertex shader for light space transform
layout(std140, set = 2, binding = 0) uniform SceneLighting {
    LightData lights[16];
    vec4 ambient;
    int numLights;
    float _padding[3];
    ShadowData shadowData;
} lighting;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 customData;
} push;

void main()
{
    // Calculate world position
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Transform normal to world space (using inverse transpose for non-uniform scaling)
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    
    // Pass through texture coordinates
    fragTexCoord = inTexCoord;
    
    // Calculate light space position for shadow mapping
    fragLightSpacePos = lighting.shadowData.lightSpaceMatrix * worldPos;
    
    // Final clip space position
    gl_Position = frame.proj * frame.view * worldPos;
}
