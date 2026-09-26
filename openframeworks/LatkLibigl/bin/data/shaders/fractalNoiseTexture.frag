// Fractal noise (fBm) for Latk stroke meshes, reading value noise from
// StrokeShader::makeNoiseTexture() instead of hashing: one texture read per
// octave. The pattern follows each stroke, wraps around the tube without a
// seam, and drifts from the start of the stroke towards its end.

uniform float time;
uniform sampler2D noiseTexture;
uniform vec2 noiseTextureSize;

varying vec2 vUv;       // x: along the stroke, in tube circumferences. y: 0-1 around it.
varying vec3 vNormal;   // eye space
varying vec3 vPosition; // eye space
varying vec4 vColor;    // stroke colour

const int OCTAVES = 5;
const float FIRST_BAND = 1.0; // the first octave has 2^FIRST_BAND cells around the tube
const float SPEED = 0.3; // cells per second

// Value noise from band `band` of the noise texture, which repeats every
// 2^band cells in y. Bilinear filtering does the interpolation; smoothstepping
// the position within the cell first makes it smooth across cell edges.
float noise(vec2 p, float band) {
	float period = exp2(band);
	float firstRow = period - 1.0 + band;
	vec2 cell = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	// The row after a band's last repeats its first, so filtering across the
	// wrap needs no special case.
	vec2 texel = vec2(cell.x, mod(cell.y, period) + firstRow) + f + 0.5;
	return texture2D(noiseTexture, texel / noiseTextureSize).r;
}

// Octaves are faded to their average as their cells shrink from eight pixels
// to four, then skipped, so thin, distant strokes don't shimmer or cost full
// price. Close up, all octaves show.
float fbm(vec2 p, float footprint) {
	float sum = 0.0;
	float amplitude = 0.5;
	for (int i = 0; i < OCTAVES; i++) {
		float fade = 1.0 - smoothstep(0.125, 0.25, footprint);
		if (fade <= 0.0) {
			// The remaining octaves' amplitudes add up to 2 * amplitude.
			sum += amplitude;
			break;
		}
		sum += amplitude * mix(0.5, noise(p, FIRST_BAND + float(i)), fade);
		// Offset along the stroke so the octaves don't line up.
		p = p * 2.0 + vec2(37.0, 0.0);
		footprint *= 2.0;
		amplitude *= 0.5;
	}
	return sum;
}

void main() {
	// uv.x is in circumferences and uv.y in turns, so scaling both by the
	// cells around the tube gives square cells.
	float cells = exp2(FIRST_BAND);
	vec2 p = vUv * cells;
	p.x -= SPEED * time;
	// uv.y jumps from 1 back to 0 at the seam; measure its rate of change
	// both ways and keep the smaller.
	float dv = min(fwidth(vUv.y), fwidth(fract(vUv.y + 0.5)));
	float footprint = length(vec2(fwidth(vUv.x), dv)) * cells;
	float n = fbm(p, footprint);

	vec3 dark = vColor.rgb * 0.3;
	vec3 bright = mix(vColor.rgb, vec3(1.0), 0.35);
	vec3 albedo = mix(dark, bright, smoothstep(0.25, 0.75, n));

	vec3 normal = normalize(vNormal);
	vec3 view = normalize(-vPosition);
	vec3 lightDir = normalize(vec3(-0.4, 0.6, 0.7));
	float diffuse = max(dot(normal, lightDir), 0.0);
	float specular = pow(max(dot(normal, normalize(lightDir + view)), 0.0), 40.0);
	float rim = pow(1.0 - max(dot(normal, view), 0.0), 3.0);
	vec3 color = albedo * (0.25 + 0.75 * diffuse + 0.3 * rim) + 0.3 * specular * n;
	gl_FragColor = vec4(color, vColor.a);
}
