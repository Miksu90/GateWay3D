#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec2 textureScale = vec2(1.0, 1.0);
uniform float textureRotation = 0.0;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    // Apply rotation to texture coordinates
    vec2 rotatedTexCoord = aTexCoord;
    if (textureRotation != 0.0) {
        // Rotate around center (0.5, 0.5)
        vec2 center = vec2(0.5, 0.5);
        rotatedTexCoord -= center;
        
        // Apply rotation matrix
        float s = sin(textureRotation);
        float c = cos(textureRotation);
        rotatedTexCoord = vec2(
            rotatedTexCoord.x * c - rotatedTexCoord.y * s,
            rotatedTexCoord.x * s + rotatedTexCoord.y * c
        );
        
        // Move back from center
        rotatedTexCoord += center;
    }
    
    // Apply scale after rotation
    TexCoord = rotatedTexCoord * textureScale;
    // Calculate TBN matrix for normal mapping
    vec3 T = normalize(mat3(model) * aTangent);
    vec3 B = normalize(mat3(model) * aBitangent);
    vec3 N = normalize(mat3(model) * aNormal);
    TBN = mat3(T, B, N);
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
