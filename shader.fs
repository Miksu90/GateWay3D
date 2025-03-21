#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in mat3 TBN;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform sampler2D wallTexture;
uniform sampler2D normalMap;
uniform sampler2D roughnessMap;
uniform bool useTexture;
uniform bool useNormalMap;
uniform bool useRoughnessMap;
uniform sampler2D texture_diffuse1;
uniform int textureType;
uniform bool flashlightOn;
uniform vec3 viewPos;
uniform vec3 flashlightPos;
uniform vec3 flashlightDir;
uniform float flashlightCutoff;
uniform float flashlightOuterCutoff;
uniform float flashlightIntensity;

void main()
{
    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    // Get normal from normal map if available
    vec3 norm;
    if(useNormalMap) {
        norm = texture(normalMap, TexCoord).rgb;
        norm = normalize(norm * 2.0 - 1.0);   // Convert from [0,1] to [-1,1]
        norm = normalize(TBN * norm);         // Convert to world space
    } else {
        norm = normalize(Normal);
    }

    // Get roughness from roughness map if available
    float roughness = 1.0;
    if(useRoughnessMap) {
        roughness = texture(roughnessMap, TexCoord).r; // Assuming single channel
    }

    // Diffuse from global light
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    // Adjust diffuse with roughness
    vec3 diffuse = diff * lightColor * roughness;

    // Specular (Blinn-Phong)
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    // Adjust specular with roughness (less specular with higher roughness)
    vec3 specular = spec * lightColor * (1.0 - roughness);

    // Flashlight (Spotlight)
    vec3 flashlightDiffuse = vec3(0.0);
    vec3 flashlightSpecular = vec3(0.0);
    if(flashlightOn) {
        vec3 flashDir = normalize(flashlightPos - FragPos);
        float theta = dot(flashDir, normalize(-flashlightDir));
        float epsilon = flashlightCutoff - flashlightOuterCutoff;
        float intensity = clamp((theta - flashlightOuterCutoff) / epsilon, 0.0, 1.0);
        
        if(theta > flashlightOuterCutoff) {
            float flashDiff = max(dot(norm, flashDir), 0.0);
            float flashSpec = pow(max(dot(norm, normalize(flashDir + viewDir)), 0.0), 32.0);
            
            flashlightDiffuse = flashDiff * lightColor * intensity * flashlightIntensity * roughness;
            flashlightSpecular = flashSpec * lightColor * intensity * flashlightIntensity * (1.0 - roughness);
        }
    }

    // Result
    vec3 result;
    if (useTexture) {
        vec3 texColor;
        if (textureType == 1) { // Model texture
            texColor = texture(texture_diffuse1, TexCoord).rgb;
        } else { // Wall texture
            texColor = texture(wallTexture, TexCoord).rgb;
        }
        result = (ambient + diffuse + specular + flashlightDiffuse + flashlightSpecular) * texColor;
    } else {
        result = (ambient + diffuse + specular + flashlightDiffuse + flashlightSpecular) * objectColor;
    }

    FragColor = vec4(result, 1.0);
}
