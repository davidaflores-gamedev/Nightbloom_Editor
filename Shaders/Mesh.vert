
#version 450

// Push constants block
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
} push;

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Output to fragment shader
layout(location = 0) out vec3 fragPos;      // World space position
layout(location = 1) out vec3 fragNormal;   // World space normal
layout(location = 2) out vec2 fragTexCoord;

void main() {
    // Transform to world space
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    
    // Transform normal to world space (using normal matrix)
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    
    // Pass through texture coordinates
    fragTexCoord = inTexCoord;
    
    // Final position: projection * view * model * position
    gl_Position = push.proj * push.view * worldPos;
}