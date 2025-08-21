
#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Output
layout(location = 0) out vec4 outColor;

// For now, simple lighting to test our matrices
// We'll upgrade this to PBR next
void main() {
    // Normalize the normal (it might have been interpolated)
    vec3 normal = normalize(fragNormal);
    
    // Simple directional light for testing
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    
    // Basic diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Ambient to see unlit parts
    vec3 ambient = vec3(0.1, 0.1, 0.1);
    
    // Use texture coordinates as color for now (to verify they work)
    vec3 baseColor = vec3(fragTexCoord, 0.5);
    
    // Combine
    vec3 result = (ambient + diffuse) * baseColor;
    
    outColor = vec4(result, 1.0);
}