//------------------------------------------------------------------------------
// Mesh.frag
//
// Fragment shader for PBR mesh rendering with shadow mapping
// 
// Descriptor Sets:
//   Set 0: Frame uniforms (view, proj, time, cameraPos)
//   Set 1: Albedo texture
//   Set 2: Scene lighting data (lights + shadow data)
//   Set 3: Shadow map sampler
//------------------------------------------------------------------------------
#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragLightSpacePos;  // NEW: Position in light space

// Output
layout(location = 0) out vec4 outColor;

// Frame uniforms
layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 time;
    vec4 cameraPos;
} frame;

// Texture sampler
layout(set = 1, binding = 0) uniform sampler2D albedoMap;

// Light data structure (must match CPU side)
struct LightData {
    vec4 position;      // xyz = pos/dir, w = type (0=dir, 1=point)
    vec4 color;         // rgb = color, a = intensity
    vec4 attenuation;   // constant, linear, quadratic, radius
};

// Shadow data structure (must match CPU side)
struct ShadowData {
    mat4 lightSpaceMatrix;
    vec4 shadowParams;  // bias, normalBias, unused, enabled
};

// Scene lighting UBO
layout(std140, set = 2, binding = 0) uniform SceneLighting {
    LightData lights[16];
    vec4 ambient;
    int numLights;
    float _padding[3];
    ShadowData shadowData;
} lighting;

// Shadow map sampler (with comparison for PCF)
layout(set = 3, binding = 0) uniform sampler2DShadow shadowMap;

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 customData;
} push;

//------------------------------------------------------------------------------
// Shadow calculation
//------------------------------------------------------------------------------
float CalculateShadow(vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
    // Check if shadows are enabled
    if (lighting.shadowData.shadowParams.w < 0.5)
        return 1.0;  // No shadow
    
    // Perform perspective divide
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    // Transform to [0,1] range (NDC is [-1,1])
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    
    // Check if outside shadow map bounds
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;  // Outside shadow map = no shadow
    }
    
    // Get bias values
    float bias = lighting.shadowData.shadowParams.x;
    float normalBias = lighting.shadowData.shadowParams.y;
    
    // Calculate slope-scaled bias based on angle between normal and light
    float cosTheta = max(dot(normal, -lightDir), 0.0);
    float slopeBias = bias * tan(acos(cosTheta));
    slopeBias = clamp(slopeBias, 0.0, bias * 2.0);
    
    // Apply combined bias
    float currentDepth = projCoords.z - slopeBias;
    
    // PCF (Percentage Closer Filtering) - 3x3 kernel
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            // texture() with sampler2DShadow returns comparison result
            shadow += texture(shadowMap, vec3(projCoords.xy + offset, currentDepth));
        }
    }
    shadow /= 9.0;
    
    return shadow;
}

//------------------------------------------------------------------------------
// Lighting calculation
//------------------------------------------------------------------------------
vec3 CalculateLighting(vec3 albedo, vec3 normal, vec3 viewDir, float shadow)
{
    vec3 result = vec3(0.0);
    
    // Ambient
    vec3 ambient = lighting.ambient.rgb * lighting.ambient.a * albedo;
    result += ambient;
    
    // Process each light
    for (int i = 0; i < lighting.numLights && i < 16; ++i)
    {
        LightData light = lighting.lights[i];
        
        vec3 lightDir;
        float attenuation = 1.0;
        float shadowFactor = 1.0;
        
        if (light.position.w < 0.5)
        {
            // Directional light
            lightDir = normalize(-light.position.xyz);
            
            // Apply shadow only to first directional light (primary shadow caster)
            if (i == 0)
            {
                shadowFactor = shadow;
            }
        }
        else
        {
            // Point light
            vec3 toLight = light.position.xyz - fragWorldPos;
            float distance = length(toLight);
            lightDir = normalize(toLight);
            
            // Attenuation
            float constant = light.attenuation.x;
            float linear = light.attenuation.y;
            float quadratic = light.attenuation.z;
            float radius = light.attenuation.w;
            
            // Skip if outside radius
            if (distance > radius)
                continue;
            
            attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);
            
            // Smooth falloff at radius edge
            float falloff = 1.0 - smoothstep(radius * 0.75, radius, distance);
            attenuation *= falloff;
        }
        
        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * light.color.rgb * light.color.a * albedo;
        
        // Specular (Blinn-Phong)
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
        vec3 specular = spec * light.color.rgb * light.color.a * 0.5;
        
        // Combine with attenuation and shadow
        result += (diffuse + specular) * attenuation * shadowFactor;
    }
    
    return result;
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------
void main()
{
    // Sample albedo texture
    vec4 albedoSample = texture(albedoMap, fragTexCoord);
    vec3 albedo = albedoSample.rgb;
    
    // Normalize interpolated normal
    vec3 normal = normalize(fragNormal);
    
    // View direction
    vec3 viewDir = normalize(frame.cameraPos.xyz - fragWorldPos);
    
    // Get primary light direction for shadow calculation
    vec3 primaryLightDir = vec3(0.0, -1.0, 0.0);
    if (lighting.numLights > 0 && lighting.lights[0].position.w < 0.5)
    {
        primaryLightDir = normalize(lighting.lights[0].position.xyz);
    }
    
    // Calculate shadow factor
    float shadow = CalculateShadow(fragLightSpacePos, normal, primaryLightDir);
    
    // Calculate final lighting
    vec3 finalColor = CalculateLighting(albedo, normal, viewDir, shadow);
    
    // Tone mapping (simple Reinhard)
    finalColor = finalColor / (finalColor + vec3(1.0));
    
    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0 / 2.2));
    
    outColor = vec4(finalColor, albedoSample.a);
}
