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
uniform bool useTexture;
uniform bool useNormalMap;

uniform sampler2D texture_diffuse1;
uniform int textureType;
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

    // Diffuse
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Result
    vec3 result;
    if (useTexture) {
        vec3 texColor;
        if (textureType == 1) {
            texColor = texture(texture_diffuse1, TexCoord).rgb;
        } else {
            texColor = texture(wallTexture, TexCoord).rgb;
        }
        result = (ambient + diffuse) * texColor;
    } else {
        result = (ambient + diffuse) * objectColor;
    }

    FragColor = vec4(result, 1.0);
}
