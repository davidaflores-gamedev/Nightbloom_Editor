#version 450

// ---- Descriptor Set 0: Frame Uniforms ----
layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 view;
    mat4 proj;
    vec4 time;
    vec4 cameraPos;    // xyz = world-space camera position
} frame;

// ---- Descriptor Set 1: Textures ----
layout(set = 1, binding = 0) uniform sampler2D texSampler;

// ---- Descriptor Set 2: Scene Lighting ----
struct LightData {
    vec4 position;      // xyz = direction (directional) or position (point), w = type (0=dir, 1=point)
    vec4 color;         // rgb = color, a = intensity
    vec4 attenuation;   // x = constant, y = linear, z = quadratic, w = radius
};

layout(std140, set = 2, binding = 0) uniform SceneLighting {
    LightData lights[16];
    vec4 ambient;       // rgb = color, a = intensity
    int numLights;
} lighting;

// ---- Push Constants ----
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 customData;    // rgb = tint, a = alpha; w > 0.01 signals material-color driven (glass)
} push;

// ---- Fragment Inputs ----
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

// ---- Output ----
layout(location = 0) out vec4 outColor;

// ============================================================================
// Blinn-Phong for a single light
// Returns vec3 (diffuse + specular contribution, NOT including albedo for diffuse)
// ============================================================================
vec3 CalcBlinnPhong(vec3 N, vec3 V, vec3 lightDir, vec3 lightColor, float lightIntensity)
{
    // Diffuse
    float NdotL = max(dot(N, lightDir), 0.0);
    vec3 diffuse = lightColor * lightIntensity * NdotL;

    // Specular (Blinn-Phong half-vector)
    vec3 H = normalize(lightDir + V);
    float NdotH = max(dot(N, H), 0.0);
    float specPower = 32.0;  // Hardcoded for now; later drive from material roughness
    float spec = pow(NdotH, specPower);
    vec3 specular = lightColor * lightIntensity * spec * 0.5;  // 0.5 = specular strength

    return diffuse + specular;
}

// ============================================================================
// Main
// ============================================================================
void main()
{
    // Sample texture
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Determine albedo: material-color (customData) or texture
    bool isMaterialDriven = (push.customData.w > 0.01);
    vec4 albedo = isMaterialDriven ? push.customData : texColor;

    // Surface normal and view direction
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(frame.cameraPos.xyz - fragWorldPos);

    // ---- Accumulate lighting ----
    // Start with ambient
    vec3 totalLight = lighting.ambient.rgb * lighting.ambient.a;

    for (int i = 0; i < lighting.numLights; ++i)
    {
        LightData light = lighting.lights[i];
        float lightType = light.position.w;

        vec3 lightDir;
        float attenuation = 1.0;

        if (lightType < 0.5)
        {
            // Directional light: position.xyz stores direction FROM the light
            // Negate to get direction TO the light
            lightDir = normalize(-light.position.xyz);
        }
        else
        {
            // Point light: position.xyz is world position
            vec3 toLight = light.position.xyz - fragWorldPos;
            float dist = length(toLight);
            lightDir = toLight / max(dist, 0.0001);  // normalize safely

            // Skip if beyond radius
            float radius = light.attenuation.w;
            if (dist > radius) continue;

            // Attenuation: 1 / (constant + linear*d + quadratic*d*d)
            float c = light.attenuation.x;
            float l = light.attenuation.y;
            float q = light.attenuation.z;
            attenuation = 1.0 / (c + l * dist + q * dist * dist);

            // Smooth cutoff at radius edge
            float fade = 1.0 - smoothstep(radius * 0.75, radius, dist);
            attenuation *= fade;
        }

        // Blinn-Phong contribution
        vec3 contribution = CalcBlinnPhong(N, V, lightDir, light.color.rgb, light.color.a);
        totalLight += contribution * attenuation;
    }

    // ---- Glass path ----
    if (isMaterialDriven)
    {
        // Fresnel: edges reflect more
        float NdotV = clamp(dot(N, V), 0.0, 1.0);
        float fresnel = pow(1.0 - NdotV, 5.0);

        vec3 tint = albedo.rgb;
        vec3 reflectionColor = vec3(1.0);  // Fake sky reflection

        float reflectStrength = 0.15 + 0.85 * fresnel;
        vec3 glassRgb = mix(tint * totalLight, reflectionColor, reflectStrength);

        outColor = vec4(glassRgb, albedo.a);
        return;
    }

    // ---- Opaque path ----
    outColor = vec4(albedo.rgb * totalLight, albedo.a);
}
