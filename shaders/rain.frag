#version 330 core

in float lenColorScale;

out vec4 fragColor;

uniform bool snowing;

void main() {
    if(snowing) {
        vec2 vecFromCenter = (gl_PointCoord - vec2(.5, .5)) * 2;
        float distance = sqrt(dot(vecFromCenter, vecFromCenter));
        fragColor = vec4(1.0, 1.0, 1.0, 1.0 - distance);
        return;
    }

    fragColor = vec4(0.5, 0.5, 0.8, 1.0 * lenColorScale);
}
