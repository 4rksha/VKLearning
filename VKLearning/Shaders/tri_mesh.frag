#version 460


layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 vTexCoord;
layout (location = 4) in vec3 worldPosition;


layout(set = 0, binding = 0) uniform CameraBuffer
{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec3 cameraPosition;
} cameraData;

layout(set = 0, binding = 1) uniform  SceneData{
    vec3 lightDirection; 
	int lightNb;
	vec3 lightColor;
	float metallic; 
	vec3 albedo;
	float roughness;
} sceneData;

struct Light{
	vec3 color;
	float intensity;
	vec3 position;
};

layout(std430, set = 1, binding = 1) readonly buffer LightBuffer
{
	Light lights[];
} lightBuffer;


layout (location = 0) out vec4 outFragColor;

/* ****************************************************** */

#define PI 3.1415926538
#define EPSILON 0.0001

vec3 fresnelSchlick(float cosTheta, vec3 F0);
float distributionGGX(vec3 n, vec3 h, float roughness);
float geometrySchlickGGX(float nDotv, float roughness);
float geometrySmith(vec3 n, vec3 v, vec3 l, float roughness);

void main()
{
	vec3 n = normalize(inNormal);
	vec3 v = normalize(cameraData.cameraPosition - worldPosition);

	vec3 l0 = vec3(0.);

	for(int i = 0; i < sceneData.lightNb; ++i) 
	{	
		Light light = lightBuffer.lights[i];

		vec3 l = normalize(light.position - inPosition);
		vec3 h = normalize(v + l);

		float distance = length(light.position[i] - worldPosition);
		float attenuation = 1. / (distance * distance);
		vec3 radiance = light.color * attenuation * light.intensity;

		vec3 f0 = mix(vec3(0.04), sceneData.albedo, sceneData.metallic); 

		// Cook-Terrance BRDF
		float ndf = distributionGGX(n, h, sceneData.roughness);
		float g = geometrySmith(n, v, l, sceneData.roughness);
		vec3 f = fresnelSchlick(max(dot(h, v), 0.), f0);

		vec3 num = ndf * g * f;
		float nDotl =  max(dot(n, l), 0.);
		float denom = 4. * max(dot(n, v), 0.) * nDotl + EPSILON;
		vec3 specular = num / denom;

		vec3 kS = f;
		vec3 kD = vec3(1.) - kS;
		kD *= 1. - sceneData.metallic;

		l0 += (kD * sceneData.albedo / PI + specular) * radiance * nDotl;
	}
	l0 = l0 / (l0 + vec3(1.));
	l0 = pow(l0, vec3(1./2.2));

	outFragColor = vec4(l0, 1.); // * vec4(sceneData.albedo, 1.);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
	return f0 + (1. - f0) * pow(clamp(1.0 - cosTheta, 0., 1.), 5.);
}

float distributionGGX(vec3 n, vec3 h, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float nDoth = max(dot(n, h), 0.);
	float nDoth2 = nDoth * nDoth;

	float f = (nDoth2 * (a2 - 1.) + 1.);
	return a2 / (PI * f * f);
}

float geometrySchlickGGX(float nDotv, float roughness)
{
	float r = roughness + 1.;
	float k = r * r / 8.;
	
	return nDotv / (nDotv * (1. - k) + k);
}

float geometrySmith(vec3 n, vec3 v, vec3 l, float roughness)
{
	float nDotv = max(dot(n, v), 0.);
	float nDotl = max(dot(n, l), 0.);
	
	return geometrySchlickGGX(nDotl, roughness) * geometrySchlickGGX(nDotv, roughness);
}
