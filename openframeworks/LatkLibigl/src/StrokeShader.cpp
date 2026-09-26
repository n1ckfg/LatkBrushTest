#include "StrokeShader.h"

namespace {

const string vertexShaderGL2 = R"(#version 120
varying vec2 vUv;
varying vec3 vNormal;
varying vec3 vPosition;
varying vec4 vColor;

void main() {
	vec4 eye = gl_ModelViewMatrix * gl_Vertex;
	vPosition = eye.xyz;
	vNormal = gl_NormalMatrix * gl_Normal;
	vUv = gl_MultiTexCoord0.xy;
	vColor = gl_Color;
	gl_Position = gl_ProjectionMatrix * eye;
}
)";

// The programmable renderer has no normal matrix uniform. The model view
// matrix is a rotation and uniform scale here, so its upper 3x3 will do.
const string vertexShaderGL3 = R"(#version 150
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
in vec4 position;
in vec3 normal;
in vec2 texcoord;
in vec4 color;
out vec2 vUv;
out vec3 vNormal;
out vec3 vPosition;
out vec4 vColor;

void main() {
	vec4 eye = modelViewMatrix * position;
	vPosition = eye.xyz;
	vNormal = mat3(modelViewMatrix) * normal;
	vUv = texcoord;
	vColor = color;
	gl_Position = projectionMatrix * eye;
}
)";

const string litFragmentShader = R"(
varying vec3 vNormal;
varying vec3 vPosition;
varying vec4 vColor;

void main() {
	vec3 n = normalize(vNormal);
	vec3 v = normalize(-vPosition);
	vec3 l = normalize(vec3(-0.4, 0.6, 0.7));
	float diffuse = max(dot(n, l), 0.0);
	float specular = pow(max(dot(n, normalize(l + v)), 0.0), 40.0);
	float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0);
	vec3 color = vColor.rgb * (0.25 + 0.75 * diffuse + 0.3 * rim) + 0.3 * specular;
	gl_FragColor = vec4(color, vColor.a);
}
)";

// Drops any #version line, which has to come from the header below.
string stripVersion(const string & source) {
	string result;
	for (auto & line : ofSplitString(source, "\n")) {
		result += ofIsStringInString(ofTrim(line), "#version") ? "\n" : line + "\n";
	}
	return result;
}

}

//--------------------------------------------------------------
bool StrokeShader::load(const string & fragmentPath) {
	string source = litFragmentShader;
	if (!fragmentPath.empty()) {
		ofFile file(fragmentPath);
		if (!file.exists()) {
			ofLogError("StrokeShader") << "Can't find " << file.getAbsolutePath();
			return false;
		}
		source = ofBufferFromFile(fragmentPath).getText();
	}

	string header;
	if (ofIsGLProgrammableRenderer()) {
		header = "#version 150\n#define varying in\n#define texture2D texture\n#define gl_FragColor fragColor\nout vec4 fragColor;\n";
	} else {
		header = "#version 120\n";
	}

	// Compile into a new shader, so a broken edit leaves the old one running.
	// Before GLSL 3.30, "#line 0" means the next line is line 1.
	ofShader next;
	const string & vertexShader = ofIsGLProgrammableRenderer() ? vertexShaderGL3 : vertexShaderGL2;
	if (!next.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader)
		|| !next.setupShaderFromSource(GL_FRAGMENT_SHADER, header + "#line 0\n" + stripVersion(source))
		|| !next.linkProgram()) {
		ofLogError("StrokeShader") << "Keeping the previous shader; " << (fragmentPath.empty() ? "the built-in shader" : fragmentPath) << " didn't compile.";
		return false;
	}
	shader = next;
	path = fragmentPath;
	return true;
}

//--------------------------------------------------------------
bool StrokeShader::reload() {
	return load(path);
}

//--------------------------------------------------------------
void StrokeShader::setTexture(const string & name, const ofTexture & texture) {
	for (auto & t : textures) {
		if (t.first == name) {
			t.second = texture;
			return;
		}
	}
	textures.emplace_back(name, texture);
}

//--------------------------------------------------------------
void StrokeShader::begin() {
	shader.begin();
	shader.setUniform1f("time", ofGetElapsedTimef());
	for (size_t i = 0; i < textures.size(); i++) {
		const ofTexture & texture = textures[i].second;
		// Unit 0 is left to openFrameworks' own drawing.
		shader.setUniformTexture(textures[i].first, texture, i + 1);
		shader.setUniform2f(textures[i].first + "Size", texture.getWidth(), texture.getHeight());
	}
}

//--------------------------------------------------------------
void StrokeShader::end() {
	shader.end();
}

//--------------------------------------------------------------
string StrokeShader::getName() const {
	return path.empty() ? "lit" : ofFilePath::getBaseName(path);
}

//--------------------------------------------------------------
ofTexture StrokeShader::makeNoiseTexture(int width, int bands) {
	int height = 0;
	for (int b = 0; b < bands; b++) height += (1 << b) + 1;

	ofPixels pixels;
	pixels.allocate(width, height, OF_PIXELS_GRAY);
	unsigned char * data = pixels.getData();
	// A fixed seed gives the same pattern every run.
	std::mt19937 random(1);
	std::uniform_int_distribution<int> value(0, 255);
	int row = 0;
	for (int b = 0; b < bands; b++) {
		const int period = 1 << b;
		for (int i = 0; i < period * width; i++) data[row * width + i] = value(random);
		std::copy(data + row * width, data + (row + 1) * width, data + (row + period) * width);
		row += period + 1;
	}

	// Rectangle textures can't repeat, so ask for GL_TEXTURE_2D.
	ofTexture texture;
	texture.allocate(pixels, false);
	texture.setTextureWrap(GL_REPEAT, GL_CLAMP_TO_EDGE);
	texture.setTextureMinMagFilter(GL_LINEAR, GL_LINEAR);
	return texture;
}
