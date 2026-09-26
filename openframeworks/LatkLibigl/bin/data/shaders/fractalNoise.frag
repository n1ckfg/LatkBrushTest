// Fractal noise (fBm) for Latk stroke meshes. The pattern is laid out in each
// stroke's uv space, so it follows the stroke, wraps around the tube without a
// seam, and drifts from the start of the stroke towards its end.

uniform float time;

varying vec2 vUv;       // x: along the stroke, in tube circumferences. y: 0-1 around it.
varying vec3 vNormal;   // eye space
varying vec3 vPosition; // eye space
varying vec4 vColor;    // stroke colour

const int OCTAVES = 5;
const float TAU = 6.2831853;

// Value noise with a hash by Inigo Quilez.
float hash(vec3 p) {
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i + vec3(0.0, 0.0, 0.0)), hash(i + vec3(1.0, 0.0, 0.0)), f.x),
	               mix(hash(i + vec3(0.0, 1.0, 0.0)), hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
	           mix(mix(hash(i + vec3(0.0, 0.0, 1.0)), hash(i + vec3(1.0, 0.0, 1.0)), f.x),
	               mix(hash(i + vec3(0.0, 1.0, 1.0)), hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y), f.z);
}

// Octaves are faded to their average as their cells shrink from eight pixels
// to four, then skipped, so thin, distant strokes don't shimmer or cost full
// price. Close up, all octaves show.
float fbm(vec3 p) {
	float footprint = length(fwidth(p));
	float sum = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < OCTAVES; i++) {
		float fade = 1.0 - smoothstep(0.125, 0.25, footprint);
		if (fade <= 0.0) {
			// The remaining octaves' amplitudes add up to 2 * amplitude.
			sum += amplitude;
			break;
		}
		sum += amplitude * mix(0.5, noise(p), fade);
		p = p * 2.03 + vec3(17.1, 3.7, 9.2);
		footprint *= 2.03;
		amplitude *= 0.5;
	}
	return sum;
}

void main() {
	// Wrap uv onto a cylinder of radius 1. Around the tube that's seamless, and
	// since uv.x is measured in circumferences, the noise isn't stretched.
	float angle = TAU * vUv.y;
	vec3 p = vec3(TAU * vUv.x - 0.8 * time, cos(angle), sin(angle)) * 1.2;
	float n = fbm(p);

	vec3 dark = vColor.rgb * 0.2;
	vec3 bright = mix(vColor.rgb, vec3(1.0), 0.3);
	vec3 albedo = mix(dark, bright, smoothstep(0.3, 0.7, n));

	vec3 normal = normalize(vNormal);
	vec3 view = normalize(-vPosition);
	vec3 lightDir = normalize(vec3(-0.4, 0.6, 0.7));
	float diffuse = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(normal, normalize(lightDir + view)), 0.0), 40.0);
	float rim = pow(1.0 - max(dot(normal, view), 0.0), 3.0);
	vec3 color = albedo * (0.25 + 0.75 * diffuse + 0.3 * rim) + 0.3 * specular * n;
	gl_FragColor = vec4(color, vColor.a);
}
