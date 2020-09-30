#version 330 core
layout (location = 0) in vec3 initPos;

uniform vec3 offsets;
uniform vec3 cameraPos; 
uniform vec3 forwardOffset;
uniform vec3 inverseDir;
uniform float boxSize;
uniform bool snowing;

uniform mat4 viewProj;
uniform mat4 prevViewProj;

out float lenColorScale;

void main()
{
	vec3 pos = mod(initPos + offsets, boxSize);
	pos += cameraPos + (forwardOffset - boxSize / 2);

	if(snowing){
		vec4 finalPos = viewProj * vec4(pos, 1.0);
		float dist = finalPos.z / boxSize;
		gl_PointSize = mix(9.0, 1.5, dist); // enabled by cpu call: GL_VERTEX_PROGRAM_POINT_SIZE
		gl_Position = finalPos;
		return;
	}

	vec4 bot_pos = viewProj * vec4(pos, 1.0);

	vec4 top_pos = viewProj * vec4(pos + inverseDir, 1.0);
	vec4 top_pos_prev = prevViewProj * vec4(pos + inverseDir, 1.0);

	vec2 dir = (top_pos.xy/top_pos.w) - (bot_pos.xy/bot_pos.w);
	vec2 dirPrev = (top_pos_prev.xy/top_pos_prev.w) - (bot_pos.xy/bot_pos.w);

	float len = length(dir);
	float lenPrev = length(dirPrev);
	
	lenColorScale = clamp(len/lenPrev, 0.0, 1.0);
	gl_Position = mod(gl_VertexID, 2) == 0 ? bot_pos : top_pos_prev;
}