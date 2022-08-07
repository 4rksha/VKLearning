#version 460

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 vTexCoord;

layout(set = 0, binding = 1) uniform  SceneData{
    vec4 fogColor; // w is for exponent
	vec4 fogDistances; //x for min, y for max, zw unused.
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout (location = 0) out vec4 outFragColor;

void main()
{
	vec3 l = normalize(vec3(0., 10., 10.f) - inPosition);
	vec3 n = normalize(inNormal);
	float cosTheta = max(0.2, dot(n, l));
	outFragColor = inColor * cosTheta;
}