#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vColor;
out vec3 vNormal;
out vec3 vFragPos;

void main() {
    vColor = aColor;
    // Transform normal by model matrix (rotation/scale only, no translation)
    // Use mat3 to extract upper-left 3x3 from model matrix
    vNormal = mat3(uModel) * normalize(aPos);
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    gl_Position = uMVP * vec4(aPos, 1.0);
}
