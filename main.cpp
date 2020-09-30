#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <chrono>
#include "shader.h"
#include "glmutils.h"
#include "primitives.h"
#include "PerlinNoise.hpp"

// structure to hold render info
// -----------------------------
struct SceneObject {
	unsigned int VAO;
	unsigned int vertexCount;
	void drawSceneObject(std::uint16_t glMode, bool hasEbo) {
		glBindVertexArray(VAO);
		if (hasEbo)
		{
			// there is probably a smarter way to do this
			glDrawElements(glMode, vertexCount, GL_UNSIGNED_INT, nullptr);
		}
		else
		{
			glDrawArrays(glMode, 0, vertexCount);
		}
	}
};

// function declarations
// ---------------------
unsigned int createArrayBuffer(const std::vector<float>& array);
unsigned int createElementArrayBuffer(const std::vector<unsigned int>& array);
unsigned int createVertexArray(const std::vector<float>& positions, const std::vector<float>& colors, const std::vector<unsigned int>& indices);
void setup();
void drawObjects();
void drawCube(glm::mat4 model);
void drawRain(glm::mat4 model);

// glfw and input functions
// ------------------------
void cursorInRange(float screenX, float screenY, int screenW, int screenH, float min, float max, float& x, float& y);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void cursor_input_callback(GLFWwindow* window, double posX, double posY);

// screen settings
// ---------------
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// global variables used for rendering
// -----------------------------------
SceneObject cube;
SceneObject floorObj;
SceneObject rain;
Shader* solidShaderProgram;
Shader* rainShaderProgram;

// global variables used for control
// ---------------------------------
bool snowMode = false;
float currentTime;
glm::vec3 camForward(.0f, .0f, -1.0f);
glm::vec3 camPosition(.0f, 1.6f, 0.0f);
float linearSpeed = 0.15f, cameraSensitivity = 25.0f;

const float boxSize = 30.f;
const unsigned int rainParticleCount = 5000;	// # of rain particles
const unsigned int rainParticleSize = 3;		// # rain values

int main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Weather Effects", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, cursor_input_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	// ------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// setup mesh objects
	// ------------------------------------
	setup();

	// set up the z-buffer
	// Notice that the depth range is now set to glDepthRange(-1,1), that is, a left handed coordinate system.
	// That is because the default openGL's NDC is in a left handed coordinate system (even though the default
	// glm and legacy openGL camera implementations expect the world to be in a right handed coordinate system);
	// so let's conform to that
	glDepthRange(-1, 1); // make the NDC a LEFT handed coordinate system, with the camera pointing towards +z
	glEnable(GL_DEPTH_TEST); // turn on z-buffer depth test
	glDepthFunc(GL_LESS); // draws fragments that are closer to the screen in NDC

	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE); // remember to add to enable the gl_PointSize built-in variable
	glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
	
	// render loop
	// -----------
	// render every loopInterval seconds
	float loopInterval = 0.02f;
	auto begin = std::chrono::high_resolution_clock::now();

	while (!glfwWindowShouldClose(window))
	{
		// update current time
		auto frameStart = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> appTime = frameStart - begin;
		currentTime = appTime.count();

		processInput(window);

		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

		// notice that we also need to clear the depth buffer (aka z-buffer) every new frame
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		drawObjects(); // draw env + rain

		glfwSwapBuffers(window);
		glfwPollEvents();

		// control render loop frequency
		std::chrono::duration<float> elapsed = std::chrono::high_resolution_clock::now() - frameStart;
		while (loopInterval > elapsed.count()) {
			elapsed = std::chrono::high_resolution_clock::now() - frameStart;
		}
	}

	delete solidShaderProgram;
	delete rainShaderProgram;

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();
	return 0;
}



void drawObjects() {
	glm::mat4 scale = glm::scale(1.f, 1.f, 1.f);

	// update the camera pose and projection
	// set the matrix that takes points in the world coordinate system and project them
	// world_to_view -> view_to_perspective_projection
	// or if we want ot match the multiplication order, we could read
	// perspective_projection_from_view <- view_from_world
	glm::mat4 projection = glm::perspectiveFovRH_NO(70.0f, (float)SCR_WIDTH, (float)SCR_HEIGHT, .01f, 100.0f);
	glm::mat4 view = glm::lookAt(camPosition, camPosition + camForward, glm::vec3(0, 1, 0));
	glm::mat4 viewProjection = projection * view;

	glDisable(GL_BLEND);
	solidShaderProgram->use();

	// draw floor 
	solidShaderProgram->setMat4("model", viewProjection);
	floorObj.drawSceneObject(GL_TRIANGLES, true);

	// draw 2 cubes
	drawCube(viewProjection * glm::translate(2.0f, 1.f, 2.0f) * glm::rotateY(glm::half_pi<float>()) * scale);
	drawCube(viewProjection * glm::translate(-2.0f, 1.f, -2.0f) * glm::rotateY(glm::quarter_pi<float>()) * scale);

	rainShaderProgram->use();
	glEnable(GL_BLEND);
	drawRain(viewProjection);
}

void drawRain(glm::mat4 viewProj)
{
	// initialize statics for the method
	static glm::mat4 prevViewProj = viewProj;
	static glm::vec3 gravity[4] = { glm::vec3(0.0f, -15.0f, 0.0f),
							glm::vec3(0.0f, -11.0f, 0.0f),
							glm::vec3(0.0f,  -10.0f, 0.0f),
							glm::vec3(0.0f, -6.0f, 0.0f) };
	static glm::vec3 wind[4] = { glm::vec3(0.6f, 0.0f, 0.79f),
								glm::vec3(0.5f, 0.0f, 0.215f),
								glm::vec3(0.42f, 0.0f, 0.9f),
								glm::vec3(.75f, 0.0f, 1.5f) };

	// if snowing:
	// - slow down gravity
	// - intensify wind slightly
	// - draw points
	const float snowFactor = !snowMode ? 1.0f : 0.1f;
	
	float noise = static_cast<float>(siv::PerlinNoise().noise1D(currentTime));
	noise = snowMode ? noise / 6.f : noise / 2.f;
	
	const glm::vec3 forwardOffset = glm::normalize(camForward * (boxSize / 2.f));
	// render the rain particles 4 times with different offsets
	for(unsigned int i = 0; i < 4; i++)
	{
		glm::vec3 gravityOffset = gravity[i] * currentTime * snowFactor;
		glm::vec3 windOffset = noise + wind[i] * currentTime * ((snowFactor + 1.0f)/2.f);

		glm::vec3 offsets = gravityOffset + windOffset;
		offsets -= camPosition + forwardOffset + boxSize / 2.f; // magic that works even if removed
		offsets = glm::mod(offsets, boxSize);
		
		rainShaderProgram->setMat4("prevViewProj", prevViewProj);
		rainShaderProgram->setMat4("viewProj", viewProj);
		rainShaderProgram->setVec3("cameraPos", camPosition);
		rainShaderProgram->setVec3("forwardOffset", forwardOffset);
		rainShaderProgram->setVec3("inverseDir", (-gravity[i]-wind[i])*0.02f);
		rainShaderProgram->setFloat("boxSize", boxSize);
		rainShaderProgram->setVec3("offsets", offsets);
		rainShaderProgram->setBool("snowing", snowMode);
		
		const int drawMode = snowMode ? GL_POINTS : GL_LINES;
		rain.drawSceneObject(drawMode, false);
	}

	prevViewProj = viewProj;
}

void drawCube(glm::mat4 model) {
	// draw object
	solidShaderProgram->setMat4("model", model);
	cube.drawSceneObject(GL_TRIANGLES, true);
}

void createRainParticles()
{
	std::cout << "Creating " << rainParticleCount << " rain particles" << std::endl;
	static const float max_rand = static_cast<float>(RAND_MAX);

	unsigned int VAO, VBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	// initialize the 10k rain particles each with 3 floats for x y z initial pos
	// two at a time with the same randomized position
	std::vector<float> data(rainParticleCount * rainParticleSize);
	for (unsigned int i = 0; i < data.size(); i = i + 6)
	{
		float v1 = (static_cast<float>(rand()) / max_rand) * boxSize;
		float v2 = (static_cast<float>(rand()) / max_rand) * boxSize;
		float v3 = (static_cast<float>(rand()) / max_rand) * boxSize;
		data[i + 0] = v1; // x_top
		data[i + 1] = v2; // y_top
		data[i + 2] = v3; // z_top
		
		data[i + 3] = v1; // x_bot
		data[i + 4] = v2; // y_bot
		data[i + 5] = v3; // z_bot
	}

	// transfer data to gpu
	glBufferData(GL_ARRAY_BUFFER, rainParticleCount * rainParticleSize * sizeof(float), &data[0], GL_DYNAMIC_DRAW);

	unsigned int initLocPos = glGetAttribLocation(rainShaderProgram->ID, "initPos");
	glEnableVertexAttribArray(initLocPos);
	glVertexAttribPointer(initLocPos, rainParticleSize, GL_FLOAT, GL_FALSE, rainParticleSize * sizeof(float), nullptr);

	rain.VAO = VAO;
	rain.vertexCount = rainParticleCount;
}

void setup() {
	// initialize shaders
	solidShaderProgram = new Shader("shaders/shader.vert", "shaders/shader.frag");
	rainShaderProgram = new Shader("shaders/rain.vert", "shaders/rain.frag");

	// load floor mesh into openGL
	floorObj.VAO = createVertexArray(floorVertices, floorColors, floorIndices);
	floorObj.vertexCount = floorIndices.size();

	// load cube mesh into openGL
	cube.VAO = createVertexArray(cubeVertices, cubeColors, cubeIndices);
	cube.vertexCount = cubeIndices.size();
	
	createRainParticles();
}

unsigned int createVertexArray(const std::vector<float>& positions, const std::vector<float>& colors, const std::vector<unsigned int>& indices) {
	unsigned int VAO;
	glGenVertexArrays(1, &VAO);
	// bind vertex array object
	glBindVertexArray(VAO);

	// set vertex shader attribute "pos"
	createArrayBuffer(positions); // creates and bind  the VBO
	int posAttributeLocation = glGetAttribLocation(solidShaderProgram->ID, "pos");
	glEnableVertexAttribArray(posAttributeLocation);
	glVertexAttribPointer(posAttributeLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);

	// set vertex shader attribute "color"
	createArrayBuffer(colors); // creates and bind the VBO
	int colorAttributeLocation = glGetAttribLocation(solidShaderProgram->ID, "color");
	glEnableVertexAttribArray(colorAttributeLocation);
	glVertexAttribPointer(colorAttributeLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);

	// creates and bind the EBO
	createElementArrayBuffer(indices);

	return VAO;
}

unsigned int createArrayBuffer(const std::vector<float>& array) {
	unsigned int VBO;
	glGenBuffers(1, &VBO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, array.size() * sizeof(GLfloat), &array[0], GL_STATIC_DRAW);

	return VBO;
}


unsigned int createElementArrayBuffer(const std::vector<unsigned int>& array) {
	unsigned int EBO;
	glGenBuffers(1, &EBO);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, array.size() * sizeof(unsigned int), &array[0], GL_STATIC_DRAW);

	return EBO;
}

void cursorInRange(float screenX, float screenY, int screenW, int screenH, float min, float max, float& x, float& y) {
	float sum = max - min;
	float xInRange = (float)screenX / (float)screenW * sum - sum / 2.0f;
	float yInRange = (float)screenY / (float)screenH * sum - sum / 2.0f;
	x = xInRange;
	y = -yInRange; // flip screen space y axis
}

void cursor_input_callback(GLFWwindow* window, double posX, double posY) {
	// rotate the camera position based on mouse movements
	// if you decide to use the lookAt function, make sure that the up vector and the
	// vector from the camera position to the lookAt target are not collinear
	static float rotationAroundVertical = 0;
	static float rotationAroundLateral = 0;

	int screenW, screenH;

	// get cursor position and scale it down to a smaller range
	glfwGetWindowSize(window, &screenW, &screenH);
	glm::vec2 cursorPosition(0.0f);
	cursorInRange(posX, posY, screenW, screenH, -1.0f, 1.0f, cursorPosition.x, cursorPosition.y);

	// initialize with first value so that there is no jump at startup
	static glm::vec2 lastCursorPosition = cursorPosition;

	// compute the cursor position change
	auto positionDiff = cursorPosition - lastCursorPosition;

	// require a minimum threshold to rotate
	if (glm::dot(positionDiff, positionDiff) > 1e-5f) {
		// rotate the forward vector around the Y axis, notices that w is set to 0 since it is a vector
		rotationAroundVertical += glm::radians(-positionDiff.x * cameraSensitivity);
		camForward = glm::rotateY(rotationAroundVertical) * glm::vec4(0, 0, -1, 0);
		// rotate the forward vector around the lateral axis
		rotationAroundLateral += glm::radians(positionDiff.y * cameraSensitivity);
		// we need to clamp the range of the rotation, otherwise forward and Y axes get parallel
		rotationAroundLateral = glm::clamp(rotationAroundLateral, -glm::half_pi<float>() * 0.9f, glm::half_pi<float>() * 0.9f);
		glm::vec3 lateralAxis = glm::cross(camForward, glm::vec3(0, 1, 0));
		camForward = glm::rotate(rotationAroundLateral, lateralAxis) * glm::rotateY(rotationAroundVertical) * glm::vec4(0, 0, -1, 0);
		camForward = glm::normalize(camForward);

		// save current cursor position
		lastCursorPosition = cursorPosition;
	}
}

void processInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	// move the camera position based on keys pressed (use either WASD or the arrow keys)
	// camera forward in the XZ plane
	glm::vec3 forwardInXZ = glm::normalize(glm::vec3(camForward.x, 0, camForward.z));
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		camPosition += forwardInXZ * linearSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
		camPosition -= forwardInXZ * linearSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		// vector perpendicular to camera forward and Y-axis
		camPosition -= glm::cross(forwardInXZ, glm::vec3(0, 1, 0)) * linearSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		// vector perpendicular to camera forward and Y-axis
		camPosition += glm::cross(forwardInXZ, glm::vec3(0, 1, 0)) * linearSpeed;
	}
	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
		snowMode = true;
	}
	if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
		snowMode = false;
	}
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}