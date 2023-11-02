#version 330

in vec2 fragTexCoord;
in vec4 fragColor;


uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 key_size;

out vec4 finalColor;

float outline_size = 1.0;
vec4 outline_color = vec4(0, 1, 0, 1);

void main() {
    
    vec2 texCoord = fragTexCoord * key_size;
    vec2 texelSize = 1.0 / key_size;

    // Calculate distance from edges
    float dx = min(texCoord.x, key_size.x - texCoord.x);
    float dy = min(texCoord.y, key_size.y - texCoord.y);
    
    if (dx <= outline_size || dy <= outline_size) {
        finalColor = outline_color;
    } else {
        // finalColor = texture(texture0, fragTexCoord);
        vec4 noiseColor = texture(texture0, fragTexCoord);
    
        float brightness = (noiseColor.x + noiseColor.y + noiseColor.z) / 3;

        if (brightness > 0.6) {
            finalColor = noiseColor;
        } else {
            finalColor = vec4(0);
        }
    }

    //float outline = smoothstep(0.0, outline_size, min(dx, dy));
    
    //finalColor = mix(outline_color, texture(texture0, fragTexCoord), outline);
}


/*
        vec4 noiseColor = texture(texture0, fragTexCoord);
    
        float brightness = (noiseColor.x + noiseColor.y + noiseColor.z) / 3;

        if (brightness > 0.6) {
            finalColor = noiseColor;
        } else {
            finalColor = vec4(0);
        }
*/
//}