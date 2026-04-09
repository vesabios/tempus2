#version 150

uniform vec3    lightDirection;
uniform vec3    lightColor;
uniform vec3    shadowColor;

in vec4	vertPosition;
in vec3	vertNormal;
in vec4 lightPos;

out vec4 fragColor;

void main()
{


	// Calculate lighting vectors.
	vec3 L = normalize( lightDirection.xyz );
	vec3 N = normalize( vertNormal );

	// Calculate diffuse lighting component.
	const vec3 kDiffuseColor = vec3( 0.4, 0.2, 0 );
    const vec3 kShadowColor = vec3( 0.1, 0.05, 0 );
    
    float NoL = dot( N, L );
    
    if (NoL>0) {
        fragColor = vec4( lightColor , 1.0 );

    } else {
        fragColor = vec4( shadowColor , 1.0 );

    }
    

}
