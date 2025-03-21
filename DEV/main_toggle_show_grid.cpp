#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <map>
#include <set>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Window dimensions
const unsigned int SCREEN_WIDTH = 800;
const unsigned int SCREEN_HEIGHT = 600;

// Camera settings
float yaw = 0.0f;    // Facing  direction initially
float pitch = 0.0f;    // Looking straight ahead
float fov = 45.0f;
float mouseSensitivity = 0.1f;
bool firstMouse = true;
float lastX = SCREEN_WIDTH / 2.0f;
float lastY = SCREEN_HEIGHT / 2.0f;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Player settings
float playerHeight = 1.0f;
float playerWidth = 0.25f;  // For collision detection
float playerSpeed = 2.5f;
bool playerOnGround = true;

// World settings
const float CELL_SIZE = 1.0f;
const float WALL_HEIGHT = 2.0f;

bool useNormalMaps = true;  // Start with normal maps enabled
bool showGrid = false;  // Show grid or not

// Shader class to handle shaders
class Shader {
public:
    // Program ID
    unsigned int ID;

    // Constructor
  Shader(const char* vertexPath, const char* fragmentPath) {
    // Read shader source from files
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;

        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vShaderFile.close();
        fShaderFile.close();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
         catch (const std::ifstream::failure& e) {  // Fixed to catch by reference
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ" << std::endl;
        }

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        // Compile shaders
        unsigned int vertex, fragment;
        int success;
        char infoLog[512];

        // Vertex shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);

        // Check for shader compile errors
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertex, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        // Fragment shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);

        // Check for shader compile errors
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        // Shader program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);

        // Check for linking errors
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(ID, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }

        // Delete shaders as they're linked into the program and no longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    // Use the shader
    void use() {
        glUseProgram(ID);
    }

    // Utility uniform functions
    void setBool(const std::string &name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void setInt(const std::string &name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setFloat(const std::string &name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setVec3(const std::string &name, const glm::vec3 &value) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }
    void setMat4(const std::string &name, const glm::mat4 &mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
};

// Camera class
class Camera {
public:
    // Camera attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // Constructor
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f)) {
        Position = position;
        WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        updateCameraVectors();
    }

    // Returns the view matrix
    glm::mat4 GetViewMatrix() {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // Updates the camera vectors based on the Euler angles
    void updateCameraVectors() {
        // Calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        Front = glm::normalize(front);

        // Also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};

// Map class to handle the world map
class Map {
public:
    std::vector<std::vector<int>> grid;
    std::vector<std::vector<int>> textureIDs;  // New field for texture IDs
    int width, height;

    // Constructor that loads a map from a file
    Map(const std::string& filename) {
        loadFromFile(filename);
    }

   void loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open map file: " << filename << std::endl;
        return;
    }

    std::string line;
    height = 0;
    width = 0;

    grid.clear();
    textureIDs.clear();

    while (std::getline(file, line)) {
        std::vector<int> row;
        std::vector<int> texRow;
        for (char c : line) {
            if (c == '#') {
                row.push_back(1);  // Wall
                texRow.push_back(0);  // Default texture ID
            } else if (c >= '1' && c <= '9') {
                row.push_back(1);  // Wall
                texRow.push_back(c - '0');  // Texture ID from 1-9
            } else if (c == '.') {
                row.push_back(0);  // Empty space
                texRow.push_back(0);  // No texture
            } else {
                row.push_back(0);  // Default to empty
                texRow.push_back(0);  // No texture
            }
        }
        if (row.size() > width) {
            width = row.size();
        }
        grid.push_back(row);
        textureIDs.push_back(texRow);
        height++;
    }

    // Debug print the entire map grid
    std::cout << "Map Grid (Width: " << width << ", Height: " << height << "):" << std::endl;
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            std::cout << grid[z][x] << " ";
        }
        std::cout << std::endl;
    }
}

    // Get texture ID for a specific wall
    int getTextureID(int x, int z) const {
        if (x < 0 || x >= width || z < 0 || z >= height) {
            return 0; // Default texture for out of bounds
        }

        return textureIDs[z][x];
    }

            // Add this method to the Map class
        bool isWall(float x, float z) const {
            int gridX = static_cast<int>(x / CELL_SIZE);
            int gridZ = static_cast<int>(z / CELL_SIZE);

            // Check bounds
            if (gridX < 0 || gridX >= width || gridZ < 0 || gridZ >= height) {
                return true; // Consider out of bounds as walls
            }

            return grid[gridZ][gridX] == 1;
        }

};

// Model for rendering cubes (walls)
class CubeModel {
public:
    unsigned int VAO, VBO;

    CubeModel() {
        // Vertex data for a cube with proper texture orientation for each face
        // Format: position(3), normal(3), texcoord(2), tangent(3), bitangent(3)
        float vertices[] = {
            // positions          // normals           // texture coords    // tangent            // bitangent
            // Front face (negative Z)
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,

            // Back face (positive Z)
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,        -1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,        -1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,        -1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,        -1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,        -1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,        -1.0f, 0.0f, 0.0f,    0.0f, 1.0f, 0.0f,

            // Left face (negative X)
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,         0.0f, 0.0f, -1.0f,   0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,         0.0f, 0.0f, -1.0f,   0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,         0.0f, 0.0f, -1.0f,   0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,         0.0f, 0.0f, -1.0f,   0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,         0.0f, 0.0f, -1.0f,   0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,         0.0f, 0.0f, -1.0f,   0.0f, 1.0f, 0.0f,

            // Right face (positive X)
             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,         0.0f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,         0.0f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,         0.0f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,         0.0f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,         0.0f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,         0.0f, 0.0f, 1.0f,    0.0f, 1.0f, 0.0f,

            // Bottom face (negative Y)
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, -1.0f,
             0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, -1.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, -1.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, -1.0f,

            // Top face (positive Y)
            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,         1.0f, 0.0f, 0.0f,    0.0f, 0.0f, 1.0f
        };

        // Generate and bind the Vertex Array Object
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Texture coordinates attribute
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // Tangent attribute
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);

        // Bitangent attribute
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(11 * sizeof(float)));
        glEnableVertexAttribArray(4);
    }

    void render() {
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    ~CubeModel() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }
};

class TextureManager {
public:
    // Store textures in a map instead of a vector for direct ID access
    std::map<int, unsigned int> textures;
    std::map<int, unsigned int> normalMaps;
    std::map<int, bool> hasNormalMap;

    void loadTexture(int textureID) {
        // Skip if already loaded
        if (textures.find(textureID) != textures.end()) {
            return;
        }

        // Try different file extensions for diffuse texture
        std::string extensions[] = {".png", ".jpg", ".jpeg"};
        bool loaded = false;

        for (const auto& ext : extensions) {
            // Construct the filename
            std::string filename = "textures/wall_" + std::to_string(textureID) + ext;

            // Load the texture
            unsigned int textureHandle;
            glGenTextures(1, &textureHandle);

            int width, height, nrChannels;
            unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0);
            if (data) {
                GLenum format;
                if (nrChannels == 1)
                    format = GL_RED;
                else if (nrChannels == 3)
                    format = GL_RGB;
                else if (nrChannels == 4)
                    format = GL_RGBA;

                glBindTexture(GL_TEXTURE_2D, textureHandle);
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                // Set texture parameters
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // Store texture in the map
                textures[textureID] = textureHandle;

                std::cout << "Loaded texture: " << filename << std::endl;
                loaded = true;
                stbi_image_free(data);
                break; // Exit the loop once a texture is successfully loaded
            }
            stbi_image_free(data); // Free data even if loading failed
        }

        if (!loaded) {
            std::cout << "Failed to load texture for ID: " << textureID << " (tried png, jpg, jpeg)" << std::endl;
            // Add a default texture or placeholder
            textures[textureID] = 0;
        }

        // Now try to load the normal map with _N suffix
        loadNormalMap(textureID);
    }

    void loadNormalMap(int textureID) {
        // Default to no normal map
        hasNormalMap[textureID] = false;

        // Try different file extensions for normal map
        std::string extensions[] = {".png", ".jpg", ".jpeg"};

        for (const auto& ext : extensions) {
            // Construct the filename with _N suffix
            std::string filename = "textures/wall_" + std::to_string(textureID) + "_N" + ext;

            // Load the texture
            unsigned int textureHandle;
            glGenTextures(1, &textureHandle);

            int width, height, nrChannels;
            unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0);
            if (data) {
                GLenum format;
                if (nrChannels == 1)
                    format = GL_RED;
                else if (nrChannels == 3)
                    format = GL_RGB;
                else if (nrChannels == 4)
                    format = GL_RGBA;

                glBindTexture(GL_TEXTURE_2D, textureHandle);
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                // Set texture parameters
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // Store normal map in the map
                normalMaps[textureID] = textureHandle;
                hasNormalMap[textureID] = true;

                std::cout << "Loaded normal map: " << filename << std::endl;
                stbi_image_free(data);
                return; // Exit once we found a valid normal map
            }
            stbi_image_free(data); // Free data even if loading failed
        }

        std::cout << "No normal map found for texture ID: " << textureID << std::endl;
    }

    // Bind textures with normal map support
    void bindTexture(int textureID) {
        // If the texture doesn't exist yet, try to load it
        if (textures.find(textureID) == textures.end()) {
            loadTexture(textureID);
        }

        // Bind the color texture to texture unit 0
        glActiveTexture(GL_TEXTURE0);
        if (textures.find(textureID) != textures.end() && textures[textureID] != 0) {
            glBindTexture(GL_TEXTURE_2D, textures[textureID]);
        } else {
            // Bind a default texture or do nothing
            //std::cout << "No color texture found for ID: " << textureID << std::endl;
        }

        // Bind the normal map to texture unit 1 if available
        glActiveTexture(GL_TEXTURE1);
        if (hasNormalMap[textureID]) {
            glBindTexture(GL_TEXTURE_2D, normalMaps[textureID]);
        } else {
            // Unbind if no normal map
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Reset active texture
        glActiveTexture(GL_TEXTURE0);
    }

    bool hasNormalMapForTexture(int textureID) {
        return hasNormalMap[textureID];
    }

    // Preload all textures needed for a map
    void preloadMapTextures(const Map& map) {
        std::set<int> uniqueTextureIDs;

        // Collect all unique texture IDs from the map
        for (int z = 0; z < map.height; z++) {
            for (int x = 0; x < map.width; x++) {
                int texID = map.getTextureID(x, z);
                if (texID > 0) {
                    uniqueTextureIDs.insert(texID);
                }
            }
        }

        // Load each unique texture and its normal map if available
        for (int texID : uniqueTextureIDs) {
            loadTexture(texID);
        }
    }
};


// Here's a fixed version of the checkCollision function that uses the const-correct isWall method
bool checkCollision(const glm::vec3& position, const Map& map, float radius) {
    float x = position.x;
    float z = position.z;

    // Check center point and 4 cardinal directions
    if (map.isWall(x, z)) return true;
    if (map.isWall(x + radius, z)) return true;
    if (map.isWall(x - radius, z)) return true;
    if (map.isWall(x, z + radius)) return true;
    if (map.isWall(x, z - radius)) return true;

    // Add diagonal checks
    if (map.isWall(x + radius, z + radius)) return true;
    if (map.isWall(x + radius, z - radius)) return true;
    if (map.isWall(x - radius, z + radius)) return true;
    if (map.isWall(x - radius, z - radius)) return true;

    return false;
}

// Ray-Box intersection test for more precise collision detection
bool rayBoxIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                        const glm::vec3& boxMin, const glm::vec3& boxMax,
                        float& tMin, float& tMax) {
    // Compute the intersection parameters
    glm::vec3 invDir = 1.0f / rayDir;
    glm::vec3 tMinVec = (boxMin - rayOrigin) * invDir;
    glm::vec3 tMaxVec = (boxMax - rayOrigin) * invDir;

    // Handle negative directions
    if (invDir.x < 0.0f) std::swap(tMinVec.x, tMaxVec.x);
    if (invDir.y < 0.0f) std::swap(tMinVec.y, tMaxVec.y);
    if (invDir.z < 0.0f) std::swap(tMinVec.z, tMaxVec.z);

    // Find the largest tMin and smallest tMax
    tMin = std::max(std::max(tMinVec.x, tMinVec.y), tMinVec.z);
    tMax = std::min(std::min(tMaxVec.x, tMaxVec.y), tMaxVec.z);

    // Check for intersection
    return tMax >= tMin && tMax >= 0.0f;
}

// Swept sphere collision detection against a cell-based world
bool sweptSphereCollision(const glm::vec3& start, const glm::vec3& end,
                          const Map& map, float radius, glm::vec3& adjustedEnd) {
    // Direction and distance of movement
    glm::vec3 dir = end - start;
    float dist = glm::length(dir);

    // If not moving, no collision
    if (dist < 0.0001f) {
        adjustedEnd = end;
        return false;
    }

    // Normalize direction
    dir = dir / dist;

    // Check the cells along the path
    float t = 0.0f;

    // Create a bounding box around the sphere
    float cellSize = CELL_SIZE;

    // Check the cells within a certain distance from the path
    const int checkDistance = 2; // Check up to 2 cells away

    // Get the minimum and maximum cell coordinates to check
    int startX = static_cast<int>((start.x - radius) / cellSize) - checkDistance;
    int startZ = static_cast<int>((start.z - radius) / cellSize) - checkDistance;
    int endX = static_cast<int>((end.x + radius) / cellSize) + checkDistance;
    int endZ = static_cast<int>((end.z + radius) / cellSize) + checkDistance;

    // Clamp to map boundaries
    startX = std::max(0, startX);
    startZ = std::max(0, startZ);
    endX = std::min(map.width - 1, endX);
    endZ = std::min(map.height - 1, endZ);

    bool collision = false;
    float closestT = 1.0f; // Normalized distance along the path (0 to 1)

    // Check each potential wall cell
    for (int z = startZ; z <= endZ; z++) {
        for (int x = startX; x <= endX; x++) {
            if (map.grid[z][x] == 1) { // If this is a wall
                // Create a box for this cell
                glm::vec3 boxMin(x * cellSize, start.y - radius, z * cellSize);
                glm::vec3 boxMax(boxMin.x + cellSize, start.y + radius, boxMin.z + cellSize);

                // Check if the sphere might intersect this box
                // We need to expand the box by the radius of the sphere
                boxMin -= glm::vec3(radius);
                boxMax += glm::vec3(radius);

                float tMin, tMax;
                if (rayBoxIntersection(start, dir, boxMin, boxMax, tMin, tMax) && tMin < dist) {
                    // If collision is closer than previous closest
                    if (tMin < closestT * dist) {
                        closestT = tMin / dist;
                        collision = true;
                    }
                }
            }
        }
    }

    // If collision occurred, adjust the end point
    if (collision) {
        // Move slightly less than the collision point
        closestT = std::max(0.0f, closestT - 0.01f);
        adjustedEnd = start + dir * dist * closestT;
    } else {
        adjustedEnd = end;
    }

    return collision;
}

// Most robust collision detection using circle vs. grid cells with continuous checking
bool checkCollisionCircle(const glm::vec3& position, const Map& map, float radius) {
    // Get the grid cell that contains the center of the circle
    int centerX = static_cast<int>(position.x / CELL_SIZE);
    int centerZ = static_cast<int>(position.z / CELL_SIZE);

    // Check all cells that could possibly intersect with the circle
    int radiusCells = static_cast<int>(std::ceil(radius / CELL_SIZE)) + 1;

    for (int z = centerZ - radiusCells; z <= centerZ + radiusCells; z++) {
        for (int x = centerX - radiusCells; x <= centerX + radiusCells; x++) {
            // Skip out-of-bounds cells (they're handled as walls by map.isWall)
            if (x < 0 || x >= map.width || z < 0 || z >= map.height) continue;

            // Skip empty cells
            if (map.grid[z][x] == 0) continue;

            // If this is a wall, check distance from player to cell edges

            // Calculate the closest point on the cell to the circle center
            float closestX = std::max(static_cast<float>(x * CELL_SIZE),
                            std::min(position.x, static_cast<float>((x + 1) * CELL_SIZE)));
            float closestZ = std::max(static_cast<float>(z * CELL_SIZE),
                            std::min(position.z, static_cast<float>((z + 1) * CELL_SIZE)));

            // Calculate distance squared (avoid square root for performance)
            float distanceX = position.x - closestX;
            float distanceZ = position.z - closestZ;
            float distanceSquared = distanceX * distanceX + distanceZ * distanceZ;

            // Check if the closest point is within the circle's radius
            if (distanceSquared < radius * radius) {
                return true; // Collision detected
            }
        }
    }

    return false;
}

bool collideWithMap(const glm::vec3& position, const Map& map, float radius) {
    // Convert world coordinates to grid coordinates
    int gridX = static_cast<int>(position.x / CELL_SIZE);
    int gridZ = static_cast<int>(position.z / CELL_SIZE);

    // Bounds checking
    if (gridX < 0 || gridX >= map.width || gridZ < 0 || gridZ >= map.height) {
        return true; // Out of bounds is a collision
    }

    // Check surrounding cells for potential collisions
    const int checkRadius = static_cast<int>(std::ceil(radius / CELL_SIZE)) + 1;

    for (int dz = -checkRadius; dz <= checkRadius; dz++) {
        for (int dx = -checkRadius; dx <= checkRadius; dx++) {
            int checkX = gridX + dx;
            int checkZ = gridZ + dz;

            // Skip out-of-bounds cells
            if (checkX < 0 || checkX >= map.width || checkZ < 0 || checkZ >= map.height) {
                continue;
            }

            // If it's a wall cell, do precise boundary check
            if (map.grid[checkZ][checkX] == 1) {
                float cellMinX = checkX * CELL_SIZE;
                float cellMaxX = cellMinX + CELL_SIZE;
                float cellMinZ = checkZ * CELL_SIZE;
                float cellMaxZ = cellMinZ + CELL_SIZE;

                // Precise point-box collision check
                bool collideX = position.x + radius > cellMinX && position.x - radius < cellMaxX;
                bool collideZ = position.z + radius > cellMinZ && position.z - radius < cellMaxZ;

                if (collideX && collideZ) {
                    return true;
                }
            }
        }
    }

    return false;
}
// Process movement with very small steps to prevent any chance of corner penetration
void processMovement(Camera& camera, const Map& map, float deltaTime) {
    // Debug print for precise positioning
    int gridX = static_cast<int>(camera.Position.x / CELL_SIZE);
    int gridZ = static_cast<int>(camera.Position.z / CELL_SIZE);

    float cellMinX = gridX * CELL_SIZE;
    float cellMaxX = cellMinX + CELL_SIZE;
    float cellMinZ = gridZ * CELL_SIZE;
    float cellMaxZ = cellMinZ + CELL_SIZE;



    // Rest of the existing movement code...
    glm::vec3 moveDir(0.0f);

    // Get keyboard state
    if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_W) == GLFW_PRESS) {
        glm::vec3 front = camera.Front;
        front.y = 0.0f; // Restrict to XZ plane
        moveDir += glm::normalize(front);
    }
    if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_S) == GLFW_PRESS) {
        glm::vec3 front = camera.Front;
        front.y = 0.0f; // Restrict to XZ plane
        moveDir -= glm::normalize(front);
    }
    if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_A) == GLFW_PRESS) {
        glm::vec3 right = camera.Right;
        right.y = 0.0f; // Restrict to XZ plane
        moveDir -= glm::normalize(right);
    }
    if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_D) == GLFW_PRESS) {
        glm::vec3 right = camera.Right;
        right.y = 0.0f; // Restrict to XZ plane
        moveDir += glm::normalize(right);
    }

    // If we're not moving, return early
    if (glm::length(moveDir) < 0.0001f) {
        return;
    }

    // Normalize to get direction
    moveDir = glm::normalize(moveDir);

    // Get total distance to move
    float totalDistance = playerSpeed * deltaTime;

    // Break the movement into very small steps
    const int NUM_STEPS = 30;
    float stepSize = totalDistance / NUM_STEPS;

    // Move step by step with progressive collision checking
    for (int step = 0; step < NUM_STEPS; step++) {
        // Calculate next position
        glm::vec3 nextPos = camera.Position + moveDir * stepSize;

        // Extremely conservative collision check
        if (!collideWithMap(nextPos, map, playerWidth)) {
            // If no collision, move there
            camera.Position = nextPos;
        } else {
            // If collision detected, minimize movement
            break;
        }
    }
}

// Backup code for rendering a debug circle around the player (visualization aid)
void renderDebugCircle(Shader& shader, const Camera& camera, float radius) {
    // Define a circle in world space
    const int numSegments = 32;
    std::vector<glm::vec3> circlePoints;

    for (int i = 0; i < numSegments; i++) {
        float angle = 2.0f * M_PI * i / numSegments;
        float x = camera.Position.x + radius * cos(angle);
        float z = camera.Position.z + radius * sin(angle);
        circlePoints.push_back(glm::vec3(x, camera.Position.y, z));

        angle = 2.0f * M_PI * (i + 1) / numSegments;
        x = camera.Position.x + radius * cos(angle);
        z = camera.Position.z + radius * sin(angle);
        circlePoints.push_back(glm::vec3(x, camera.Position.y, z));
    }

    // Create and setup VBO, VAO
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, circlePoints.size() * sizeof(glm::vec3), &circlePoints[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    // Render the circle
    glm::mat4 model = glm::mat4(1.0f);
    shader.setMat4("model", model);
    shader.setVec3("objectColor", glm::vec3(1.0f, 0.0f, 0.0f)); // Red circle

    glBindVertexArray(VAO);
    glDrawArrays(GL_LINES, 0, circlePoints.size());

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

// Mouse callback for camera rotation
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // Reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Make sure that when pitch is out of bounds, screen doesn't get flipped
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

// GLFW error callback
void error_callback(int error, const char* description) {
    std::cerr << "GLFW Error: " << description << std::endl;
}

// Callback for framebuffer resize
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Process escape key to close the application
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Add the N key toggle for normal maps
    static bool nKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
        if (!nKeyPressed) {
            useNormalMaps = !useNormalMaps;
            std::cout << "Normal mapping " << (useNormalMaps ? "enabled" : "disabled") << std::endl;
            nKeyPressed = true;
        }
    } else {
        nKeyPressed = false;
    }
       // Add the G key toggle for grid
    static bool gKeyPressed = false;
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
        if (!gKeyPressed) {
            showGrid = !showGrid;
            std::cout << "Grid " << (showGrid ? "enabled" : "disabled") << std::endl;
            gKeyPressed = true;
        }
    } else {
        gKeyPressed = false;
    }
}

void renderGrid(Shader& shader, const Map& map) {
    // Vertex data for grid lines
    std::vector<glm::vec3> gridLines;

    // Slightly lift the grid above the floor to prevent z-fighting
    float gridHeight = 0.01f;

    // Horizontal lines
    for (int z = 0; z <= map.height; z++) {
        gridLines.push_back(glm::vec3(0.0f, gridHeight, z * CELL_SIZE));
        gridLines.push_back(glm::vec3(map.width * CELL_SIZE, gridHeight, z * CELL_SIZE));
    }

    // Vertical lines
    for (int x = 0; x <= map.width; x++) {
        gridLines.push_back(glm::vec3(x * CELL_SIZE, gridHeight, 0.0f));
        gridLines.push_back(glm::vec3(x * CELL_SIZE, gridHeight, map.height * CELL_SIZE));
    }

    // Create and setup VBO, VAO
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, gridLines.size() * sizeof(glm::vec3), gridLines.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    // Render the grid
    glm::mat4 model = glm::mat4(1.0f);
    shader.setMat4("model", model);
    shader.setVec3("objectColor", glm::vec3(0.5f, 0.5f, 0.5f)); // Lighter gray for visibility

    // Temporarily disable depth testing to ensure grid is visible
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(VAO);
    glLineWidth(1.5f); // Make lines slightly thicker
    glDrawArrays(GL_LINES, 0, gridLines.size());
    glLineWidth(1.0f); // Reset line width

    // Re-enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

// Modify the main rendering loop to include grid rendering
// In the main rendering section of the main() function, add:
//renderGrid(shader, map);


// Create a default map.txt file if it doesn't exist
void createDefaultMapFile() {

       // Check if the file already exists
    std::ifstream testFile("map.txt");
    if (testFile.is_open()) {
        // File exists, don't overwrite it
        testFile.close();
        return;
    }

       // If we get here, the file doesn't exist, so create it
    std::ofstream file("map.txt");
    if (file.is_open()) {
        file << "########################\n";
        file << "#......................#\n";
        file << "#.....11.......22.....#\n";
        file << "#.....1............2..#\n";
        file << "#......11..........2..#\n";
        file << "#.......1.............#\n";
        file << "#.......1.............#\n";
        file << "#.......1.............#\n";
        file << "#.......11............#\n";
        file << "#.........33..........#\n";
        file << "#.....................#\n";
        file << "#.....................#\n";
        file << "#.........3...........#\n";
        file << "#.........3...........#\n";
        file << "#.........3...........#\n";
        file << "#.........3...........#\n";
        file << "#..........33.........#\n";
        file << "#.....................#\n";
        file << "#.....................#\n";
        file << "########################\n";
        file.close();
    }
}

// In createShaderFiles(), update the vertex shader:
void createShaderFiles() {
    // Vertex shader
    std::ofstream vShader("shader.vs");
    if (vShader.is_open()) {
        vShader << "#version 330 core\n";
        vShader << "layout (location = 0) in vec3 aPos;\n";
        vShader << "layout (location = 1) in vec3 aNormal;\n";
        vShader << "layout (location = 2) in vec2 aTexCoord;\n";
        vShader << "layout (location = 3) in vec3 aTangent;\n";
        vShader << "layout (location = 4) in vec3 aBitangent;\n\n";

        vShader << "out vec3 FragPos;\n";
        vShader << "out vec3 Normal;\n";
        vShader << "out vec2 TexCoord;\n";
        vShader << "out mat3 TBN;\n\n";

        vShader << "uniform mat4 model;\n";
        vShader << "uniform mat4 view;\n";
        vShader << "uniform mat4 projection;\n\n";

        vShader << "void main()\n";
        vShader << "{\n";
        vShader << "    FragPos = vec3(model * vec4(aPos, 1.0));\n";
        vShader << "    Normal = mat3(transpose(inverse(model))) * aNormal;\n";
        vShader << "    TexCoord = aTexCoord;\n";

        vShader << "    // Calculate TBN matrix for normal mapping\n";
        vShader << "    vec3 T = normalize(mat3(model) * aTangent);\n";
        vShader << "    vec3 B = normalize(mat3(model) * aBitangent);\n";
        vShader << "    vec3 N = normalize(mat3(model) * aNormal);\n";
        vShader << "    TBN = mat3(T, B, N);\n";

        vShader << "    gl_Position = projection * view * vec4(FragPos, 1.0);\n";
        vShader << "}\n";
        vShader.close();
    }

    // Update fragment shader to support normal mapping
    std::ofstream fShader("shader.fs");
    if (fShader.is_open()) {
        fShader << "#version 330 core\n";
        fShader << "out vec4 FragColor;\n\n";

        fShader << "in vec3 FragPos;\n";
        fShader << "in vec3 Normal;\n";
        fShader << "in vec2 TexCoord;\n";
        fShader << "in mat3 TBN;\n\n";

        fShader << "uniform vec3 lightPos;\n";
        fShader << "uniform vec3 lightColor;\n";
        fShader << "uniform vec3 objectColor;\n";
        fShader << "uniform sampler2D wallTexture;\n";
        fShader << "uniform sampler2D normalMap;\n";
        fShader << "uniform bool useTexture;\n";
        fShader << "uniform bool useNormalMap;\n\n";

        fShader << "void main()\n";
        fShader << "{\n";
        fShader << "    // Ambient\n";
        fShader << "    float ambientStrength = 0.3;\n";
        fShader << "    vec3 ambient = ambientStrength * lightColor;\n\n";

        fShader << "    // Get normal from normal map if available\n";
        fShader << "    vec3 norm;\n";
        fShader << "    if(useNormalMap) {\n";
        fShader << "        norm = texture(normalMap, TexCoord).rgb;\n";
        fShader << "        norm = normalize(norm * 2.0 - 1.0);   // Convert from [0,1] to [-1,1]\n";
        fShader << "        norm = normalize(TBN * norm);         // Convert to world space\n";
        fShader << "    } else {\n";
        fShader << "        norm = normalize(Normal);\n";
        fShader << "    }\n\n";

        fShader << "    // Diffuse\n";
        fShader << "    vec3 lightDir = normalize(lightPos - FragPos);\n";
        fShader << "    float diff = max(dot(norm, lightDir), 0.0);\n";
        fShader << "    vec3 diffuse = diff * lightColor;\n\n";

        fShader << "    // Result\n";
        fShader << "    vec3 result;\n";
        fShader << "    if (useTexture) {\n";
        fShader << "        vec3 texColor = texture(wallTexture, TexCoord).rgb;\n";
        fShader << "        result = (ambient + diffuse) * texColor;\n";
        fShader << "    } else {\n";
        fShader << "        result = (ambient + diffuse) * objectColor;\n";
        fShader << "    }\n\n";

        fShader << "    FragColor = vec4(result, 1.0);\n";
        fShader << "}\n";
        fShader.close();
    }
}

int main() {
    // Initialize GLFW
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
 GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Wolfenstein 3D Style Game", NULL, NULL);
if (window == NULL) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
}
glfwMakeContextCurrent(window);

// Set callbacks
glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
glfwSetCursorPosCallback(window, mouse_callback);

// Capture mouse
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

// Initialize GLEW
glewExperimental = GL_TRUE; // Enable experimental features
GLenum err = glewInit();
if (err != GLEW_OK) {
    std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
    glfwTerminate();
    return -1;
}

// Print OpenGL version information
std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

// Enable depth testing
glEnable(GL_DEPTH_TEST);

    // Create default map and shader files if they don't exist
    createDefaultMapFile();
    createShaderFiles();


    // Initialize camera
    Camera camera(glm::vec3(1.5f, playerHeight, 1.5f));


    //Textures
    TextureManager textureManager;
    // Load map
    Map map("map.txt");

    // Preload all textures needed for the map
    textureManager.preloadMapTextures(map);

    // Load shaders
    Shader shader("shader.vs", "shader.fs");
    shader.use();
    shader.setInt("wallTexture", 0); // Texture unit 0
    shader.setInt("normalMap", 1);   // Texture unit 1


    // Create cube model
    CubeModel cubeModel;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Per-frame time logic
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Process input
        processInput(window);
        processMovement(camera, map, deltaTime);

        // Update camera orientation based on mouse movement
        camera.updateCameraVectors();

        // Render
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activate shader
        shader.use();

        // Set uniforms
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        // Set lighting
        shader.setVec3("lightPos", glm::vec3(map.width * 0.5f, 5.0f, map.height * 0.5f));
        shader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));

        // Render the map
        for (int z = 0; z < map.height; ++z) {
            for (int x = 0; x < map.width; ++x) {
                if (map.grid[z][x] == 1) {  // Wall
                    int texID = map.getTextureID(x, z);

                    // This will bind both the color texture and normal map if available
                    textureManager.bindTexture(texID);

                    shader.setBool("useTexture", texID > 0);
                    // Only use normal map if both available AND the toggle is on
                    shader.setBool("useNormalMap", useNormalMaps && textureManager.hasNormalMapForTexture(texID));

                    if (texID == 0) {
                        // Fallback to color for walls without texture
                        shader.setVec3("objectColor", glm::vec3(0.7f, 0.7f, 0.7f));
                    }

                    glm::mat4 model = glm::mat4(1.0f);

       //************ THIS CAUSED THE COLLISION DETECTION TO NOT WORK, WE HAD 0,5 diffrence with the game world grid ***********
              // Translate to the center of the grid cell, not the corner
                    model = glm::translate(model, glm::vec3(
                        (x + 0.5f) * CELL_SIZE,
                        WALL_HEIGHT * 0.5f,
                        (z + 0.5f) * CELL_SIZE
                    ));
                    model = glm::scale(model, glm::vec3(CELL_SIZE, WALL_HEIGHT, CELL_SIZE));
                    shader.setMat4("model", model);


                    cubeModel.render();
                }
            }
        }

        // Render floor
        glm::mat4 floorModel = glm::mat4(1.0f);
        floorModel = glm::translate(floorModel, glm::vec3(map.width * CELL_SIZE * 0.5f, 0.0f, map.height * CELL_SIZE * 0.5f));
        floorModel = glm::scale(floorModel, glm::vec3(map.width * CELL_SIZE, 0.1f, map.height * CELL_SIZE));
        shader.setMat4("model", floorModel);
        shader.setVec3("objectColor", glm::vec3(0.3f, 0.3f, 0.3f)); // Dark gray floor
        cubeModel.render();

        // Render ceiling
        glm::mat4 ceilingModel = glm::mat4(1.0f);
        ceilingModel = glm::translate(ceilingModel, glm::vec3(map.width * CELL_SIZE * 0.5f, WALL_HEIGHT, map.height * CELL_SIZE * 0.5f));
        ceilingModel = glm::scale(ceilingModel, glm::vec3(map.width * CELL_SIZE, 0.1f, map.height * CELL_SIZE));
        shader.setMat4("model", ceilingModel);
        shader.setVec3("objectColor", glm::vec3(0.5f, 0.5f, 0.6f)); // Light blue-gray ceiling
        cubeModel.render();

        // UNCOMMENT TO SEE line to grid being rendered to the game world
                if (showGrid) {
                    renderGrid(shader, map);
                }

         // *** UNCOMMENT TO SEE line to render the debug circle
        //renderDebugCircle(shader, camera, playerWidth * 1.6f); // Using the same safety radius as in collideWithMap

        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glfwTerminate();
    return 0;
}
