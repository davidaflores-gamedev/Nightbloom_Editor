#version 450

// Frame-level uniform block
layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 time;
    vec4 cameraPos;
} frame;

// Push constants block
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 customData;
} push;

// Vertex attributes (MUST match pipeline attribute descriptions)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;     // PNT normal
layout(location = 2) in vec2 inTexCoord;   // PNT uv

// To fragment
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    // World position (needed for specular + point light distance)
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    // World normal (normal matrix = transpose of inverse of upper-left 3x3)
    mat3 normalMatrix = transpose(inverse(mat3(push.model)));
    fragNormal = normalize(normalMatrix * inNormal);

    fragTexCoord = inTexCoord;

    gl_Position = frame.proj * frame.view * worldPos;
}
