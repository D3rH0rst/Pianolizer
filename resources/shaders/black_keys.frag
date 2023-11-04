#version 330

in vec2 fragTexCoord;
in vec4 fragColor;


uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float perlin_treshold;

out vec4 finalColor;


void main() {
    vec4 noiseColor = texture(texture0, fragTexCoord);
   
    float brightness = (noiseColor.x + noiseColor.y + noiseColor.z) / 3;

    if (brightness < perlin_treshold) {
        finalColor = noiseColor;
    } else {
        finalColor = vec4(0, 0, 0, 1);
    }
}