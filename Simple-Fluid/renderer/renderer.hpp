#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <time.h>

#define STEP 1
#define SCR_WIDTH 1280
#define SCR_HEIGHT 720
#define pai 3.1415926f
#define toRadians(degrees) ((degrees*2.f*pai)/360.f)

struct Renderer
{
	Renderer() 
	{
		img_size = SCR_WIDTH * SCR_HEIGHT * 4;
		srand(time(0));
	}
	
	~Renderer() {
		delete img_data;
	}

	bool init() {
		auto rand_0_1 = []() -> float {
			return (float)(rand()) / RAND_MAX;
		};

		// image
		img_data = new unsigned char[img_size];
		glGenTextures(1, &texture);

		// shader
		auto vertex_path = "runtime/shader/opacity.vs";
		auto fragment_path = "runtime/shader/opacity.fs";
		init_shader(vertex_path, fragment_path, renderingProgram);

		// vao & vbo
		glGenVertexArrays(1, vao);
		glBindVertexArray(vao[0]);

		std::vector<float> pValues =
		{ -1, -1, 0,   1, -1, 0,
		  1,  1, 0,  -1,  1, 0 };
		std::vector<float> tValues =
		{ 0,0,  1,0,  1,1,  0,1 };

		unsigned int indices[] = {
		   0, 1, 3,
		   1, 2, 3
		};

		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(unsigned int), &(indices[0]), GL_STATIC_DRAW);

		// vert
		glGenBuffers(2, vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glBufferData(GL_ARRAY_BUFFER, pValues.size() * sizeof(float), &(pValues[0]), GL_STATIC_DRAW);

		// uv
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glBufferData(GL_ARRAY_BUFFER, tValues.size() * sizeof(float), &(tValues[0]), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		return true;
	}

	void render(float dt, glm::mat4& world_to_view_matrix, glm::mat4& view_to_clip_matrix) {
		// prepare funcs
		auto setMat4 = [&](const GLuint& program, const std::string& name, const glm::mat4& mat) -> void {
			glUniformMatrix4fv(glGetUniformLocation(program, name.c_str()), 1, GL_FALSE, &mat[0][0]);
		};
		auto setVec3 = [&](const GLuint& program, const std::string& name, const glm::vec3& value) -> void {
			glUniform3fv(glGetUniformLocation(program, name.c_str()), 1, &value[0]);
		};
		auto setVec3Ary = [&](const GLuint& program, const std::string& name, const int& count, const glm::vec3& value) -> void {
			glUniform3fv(glGetUniformLocation(program, name.c_str()), count, &value[0]);
		};

		for (int i = 0; i < img_size / 4; i++) {
			img_data[4 * i + 0] = (unsigned char)(255);
			img_data[4 * i + 1] = (unsigned char)(255);
			img_data[4 * i + 2] = (unsigned char)(90);
			img_data[4 * i + 3] = (unsigned char)(255);
		}

		// image upload
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glBindTexture(GL_TEXTURE_2D, 0);
		
		/* Render here */
		glUseProgram(renderingProgram);
		glBindVertexArray(vao[0]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glUniform1i(glGetUniformLocation(renderingProgram, "colorMap"), 0);
		glDisable(GL_DEPTH_TEST);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

private:
	int img_size;
	unsigned char* img_data;
	GLuint texture;
	GLuint vao[1];
	GLuint vbo[2];
	GLuint ebo;
	GLuint renderingProgram;

	// utility function for checking shader compilation/linking errors.
	// ------------------------------------------------------------------------
	void checkCompileErrors(GLuint shader, std::string type)
	{
		GLint success;
		GLchar infoLog[1024];
		if (type != "PROGRAM") {
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shader, 1024, NULL, infoLog);
				std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
		else {
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shader, 1024, NULL, infoLog);
				std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
	}

	void init_shader(const char* vertexPath, const char* fragmentPath, GLuint& ID) {
		std::string glsl_version = "#version 450\n";
		// 1. retrieve the vertex/fragment source code from filePath
		std::string vertexCode;
		std::string fragmentCode;
		std::ifstream vShaderFile;
		std::ifstream fShaderFile;
		// ensure ifstream objects can throw exceptions:
		vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try
		{
			// open files
			vShaderFile.open(vertexPath);
			fShaderFile.open(fragmentPath);
			std::stringstream vShaderStream, fShaderStream;
			// read file's buffer contents into streams
			vShaderStream << glsl_version;
			fShaderStream << glsl_version;
			vShaderStream << vShaderFile.rdbuf();
			fShaderStream << fShaderFile.rdbuf();
			// close file handlers
			vShaderFile.close();
			fShaderFile.close();
			// convert stream into string
			vertexCode = vShaderStream.str();
			fragmentCode = fShaderStream.str();
		}
		catch (std::ifstream::failure e)
		{
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
		}
		const char* vShaderCode = vertexCode.c_str();
		const char* fShaderCode = fragmentCode.c_str();
		// 2. compile shaders
		unsigned int vertex, fragment;
		int success;
		char infoLog[512];
		// vertex shader
		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vShaderCode, NULL);
		glCompileShader(vertex);
		checkCompileErrors(vertex, "VERTEX");
		// fragment Shader
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fShaderCode, NULL);
		glCompileShader(fragment);
		checkCompileErrors(fragment, "FRAGMENT");
		// shader Program
		ID = glCreateProgram();
		glAttachShader(ID, vertex);
		glAttachShader(ID, fragment);
		glLinkProgram(ID);
		checkCompileErrors(ID, "PROGRAM");
		// delete the shaders as they're linked into our program now and no longer necessery
		glDeleteShader(vertex);
		glDeleteShader(fragment);
	}
};