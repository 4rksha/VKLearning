//glsl version 4.5
#version 460

//shader input
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inTexCoord;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneData
{
	vec4 fogColor; // w is for exponent
	vec4 fogDistances; //x for min, y for max, zw unused.
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(set = 2, binding = 0) uniform sampler2D tex1;

void main()
{
	vec3 l = normalize(vec3(0., 10., 10.f) - inPosition);
	vec3 n = normalize(inNormal);
	float cosTheta = max(0.1, dot(n, l));
	vec4 texColor = texture(tex1,inTexCoord);
	if (texColor.a < 0.3)
		discard;
	outFragColor = texColor * inColor * cosTheta;
}