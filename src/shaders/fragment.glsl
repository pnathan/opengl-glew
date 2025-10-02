#version 330 core
in vec3 vColor;
in vec3 vNormal;
in vec3 vFragPos;
out vec4 FragColor;

uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform float uBrightness;
uniform int uIsEmissive; // 1 for light source, 0 for normal objects

void main() {
    // If this is an emissive object (light source), render as pure color
    if (uIsEmissive == 1) {
        FragColor = vec4(vColor, 1.0);
        return;
    }

    // Cel shading with distinct lighting levels
    vec3 lightDir = normalize(uLightPos - vFragPos);
    vec3 normal = normalize(vNormal);

    // Calculate distance attenuation
    float distance = length(uLightPos - vFragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);

    // Calculate diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);

    // Quantize lighting into discrete bands (cel shading effect)
    float celLevel;
    if (diff > 0.8) celLevel = 1.0;
    else if (diff > 0.5) celLevel = 0.7;
    else if (diff > 0.25) celLevel = 0.4;
    else celLevel = 0.2;

    // Apply cel shading, attenuation, and brightness to vertex color
    vec3 finalColor = vColor * celLevel * attenuation * uBrightness;

    // Add a bright rim light on edges
    vec3 viewDir = normalize(uViewPos - vFragPos);
    float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    rim = smoothstep(0.6, 1.0, rim);
    finalColor += rim * vec3(1.0, 1.0, 1.0) * 0.3;

    FragColor = vec4(finalColor, 1.0);
}
