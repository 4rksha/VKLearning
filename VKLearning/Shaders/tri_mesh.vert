#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec4 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec2 outTexCoord;
layout (location = 4) out vec3 outWorldPosition;



layout(set = 0, binding = 0) uniform  CameraBuffer
{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 cameraPosition;
} cameraData;

struct ObjectData{
	mat4 model;
};

layout(std430, set = 1, binding = 0) readonly buffer ObjectBuffer
{
	ObjectData objects[];
} objectBuffer;

//push constants block
layout( push_constant ) uniform constants
{
	vec4 data;
	mat4 renderMatrix;
} PushConstants;


void main()
{
	mat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
	mat4 mvMatrix = cameraData.view * modelMatrix;
	mat4 transformMatrix = cameraData.proj * mvMatrix;
	gl_Position = transformMatrix * vec4(vPosition, 1.0f);
	outPosition = (mvMatrix * vec4(vPosition, 1.0f)).xyz;
	outColor = vColor;
	outNormal = vNormal;
	outTexCoord = vTexCoord;
	outWorldPosition = vPosition;
}