// Real-Time Physics Tutorials
// Brandon Pelfrey
// SPH Fluid Simulation
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm/glm.hpp>
#include <cmath>
#include <string>
#include <algorithm>
#include <exception>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
// constants
#define SPH_NUM_PARTICLES 20000

#define SPH_PARTICLE_RADIUS 0.005f

#define SPH_WORK_GROUP_SIZE 128
// work group count is the ceiling of particle count divided by work group size
#define SPH_NUM_WORK_GROUPS ((SPH_NUM_PARTICLES + SPH_WORK_GROUP_SIZE - 1) / SPH_WORK_GROUP_SIZE)

#define CELLSIZE (5)
//measured length of the sides of the containing box
#define XSIZE (40 * CELLSIZE)//4
#define YSIZE (60 * CELLSIZE)
#define ZSIZE (60 * CELLSIZE)

float randFloat()
{
    return ((float)(rand() % 10000)) / 10000.0f;
}

GLuint compile_shader(std::string path_to_file, GLenum shader_type)
{
    GLuint shader_handle = 0;

    std::ifstream shader_file(path_to_file);
    std::stringstream tbuffer;
    tbuffer << shader_file.rdbuf();
    shader_file.close();
    std::string computeSource = tbuffer.str();

    if (!shader_file)
    {
        throw std::runtime_error("shader file load error");
    }
    //size_t shader_file_size = (size_t)shader_file.tellg();
    //std::vector<char> shader_code(shader_file_size);
    //shader_file.seekg(0);
    // shader_file.read(shader_code.data(), shader_file_size);
    //shader_file.close();

    shader_handle = glCreateShader(shader_type);

    //lShaderBinary(1, &shader_handle, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, shader_code.data(), static_cast<GLsizei>(shader_code.size()));
    //glSpecializeShader(shader_handle, "main", 0, nullptr, nullptr);
    
    const char* computeSrc = computeSource.c_str();
    glShaderSource(shader_handle, 1, &computeSrc, nullptr);
    glCompileShader(shader_handle);

    int32_t is_compiled;

    glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &is_compiled);
    
    if (is_compiled == GL_FALSE)
    {
        int32_t len = 0;
        glGetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &len);
        std::vector<GLchar> log(len);
        glGetShaderInfoLog(shader_handle, len, &len, &log[0]);
        for (const auto& el : log)
        {
            std::cout << el;
        }
        throw std::runtime_error("shader compile error");
    }
    return shader_handle;
}

void check_program_linked(GLuint shader_program_handle)
{
    int32_t is_linked = 0;
    glGetProgramiv(shader_program_handle, GL_LINK_STATUS, &is_linked);
    if (is_linked == GL_FALSE)
    {
        int32_t len = 0;
        glGetProgramiv(shader_program_handle, GL_INFO_LOG_LENGTH, &len);
        std::vector<GLchar> log(len);
        glGetProgramInfoLog(shader_program_handle, len, &len, &log[0]);

        for (const auto& el : log)
        {
            std::cout << el;
        }
        throw std::runtime_error("shader link error");
    }

}

int main()
{
    GLFWwindow* window = nullptr;
    uint64_t window_height = 1000;
    uint64_t window_length = 1000;

    std::atomic_uint64_t frame_number = 1;

    bool paused = false;

    int64_t scene_id = 0;

    // opengl
    uint32_t particle_position_vao_handle = 0;
    uint32_t render_program_handle = 0;
    uint32_t compute_program_handle[3]{ 0, 0, 0 };
    uint32_t packed_particles_buffer_handle = 0;
    

    if (!glfwInit())
    {
        throw std::runtime_error("glfw initialization failed");
    }

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    window = glfwCreateWindow(1000, 1000, "", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("window creation failed");
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // set vertical sync off
    

    glewInit();

    // set key callback
    auto key_callback = [](GLFWwindow* window, int key, int scancode, int action, int mode)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    };

    glfwSetKeyCallback(window, key_callback);


    // get version info 
    std::cout << "[INFO] OpenGL vendor: " << glGetString(GL_VENDOR) << std::endl << "[INFO] OpenGL renderer: " << glGetString(GL_RENDERER) << std::endl << "[INFO] OpenGL version: " << glGetString(GL_VERSION) << std::endl;
   
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    uint32_t vertex_shader_handle = compile_shader("particle.vert", GL_VERTEX_SHADER);
    uint32_t fragment_shader_handle = compile_shader("particle.frag", GL_FRAGMENT_SHADER);
    
    render_program_handle = glCreateProgram();
    glAttachShader(render_program_handle, fragment_shader_handle);
    glAttachShader(render_program_handle, vertex_shader_handle);
    glLinkProgram(render_program_handle);
    check_program_linked(render_program_handle);
    // delete shaders as we're done with them.
    glDeleteShader(vertex_shader_handle);
    glDeleteShader(fragment_shader_handle);

    uint32_t compute_shader_handle;
    compute_shader_handle = compile_shader("compute_density_pressure.comp", GL_COMPUTE_SHADER);
    compute_program_handle[0] = glCreateProgram();
    glAttachShader(compute_program_handle[0], compute_shader_handle);
    glLinkProgram(compute_program_handle[0]);
    check_program_linked(render_program_handle);
    glDeleteShader(compute_shader_handle);

    compute_shader_handle = compile_shader("compute_force.comp", GL_COMPUTE_SHADER);
    compute_program_handle[1] = glCreateProgram();
    glAttachShader(compute_program_handle[1], compute_shader_handle);
    glLinkProgram(compute_program_handle[1]);
    check_program_linked(render_program_handle);
    glDeleteShader(compute_shader_handle);

    compute_shader_handle = compile_shader("integrate.comp", GL_COMPUTE_SHADER);
    compute_program_handle[2] = glCreateProgram();
    glAttachShader(compute_program_handle[2], compute_shader_handle);
    glLinkProgram(compute_program_handle[2]);
    check_program_linked(render_program_handle);
    glDeleteShader(compute_shader_handle);


    // ssbo sizes
    constexpr ptrdiff_t position_ssbo_size = sizeof(glm::vec3) * SPH_NUM_PARTICLES;
    constexpr ptrdiff_t velocity_ssbo_size = sizeof(glm::vec3) * SPH_NUM_PARTICLES;
    constexpr ptrdiff_t force_ssbo_size = sizeof(glm::vec3) * SPH_NUM_PARTICLES;
    constexpr ptrdiff_t density_ssbo_size = sizeof(float) * SPH_NUM_PARTICLES;
    constexpr ptrdiff_t pressure_ssbo_size = sizeof(float) * SPH_NUM_PARTICLES;

    constexpr ptrdiff_t packed_buffer_size = position_ssbo_size + velocity_ssbo_size + force_ssbo_size + density_ssbo_size + pressure_ssbo_size;
    // ssbo offsets
    constexpr ptrdiff_t position_ssbo_offset = 0;
    constexpr ptrdiff_t velocity_ssbo_offset = position_ssbo_size;
    constexpr ptrdiff_t force_ssbo_offset = velocity_ssbo_offset + velocity_ssbo_size;
    constexpr ptrdiff_t density_ssbo_offset = force_ssbo_offset + force_ssbo_size;
    constexpr ptrdiff_t pressure_ssbo_offset = density_ssbo_offset + density_ssbo_size;

    std::vector<glm::vec3> initial_position(SPH_NUM_PARTICLES);

    //test
    for (auto i = 0, x = 0, y = 0; i < SPH_NUM_PARTICLES; i++)
    {
        if (i < 10)
        {
            initial_position[i].x = 0.32f + randFloat() * YSIZE / 1.16f;
            initial_position[i].y = 1.1f + randFloat() * ZSIZE / 1.0;

            initial_position[i].z = 0.2f + randFloat() * XSIZE / 1.16f;

            //initial_position[i].w = 1;
        }
        else
        {
            initial_position[i].x = 0.2f + randFloat() * XSIZE / 3.3f + randFloat() * XSIZE / 3.3f;
            initial_position[i].y = 05.0f + randFloat() * YSIZE / 2.3 + randFloat() * YSIZE / 2.6;

            initial_position[i].z = 0.4f + randFloat() * ZSIZE / 4.22f + randFloat() * ZSIZE / 4.22f;

            //initial_position[i].w = 1;
        }
    }

    void* initial_data = std::malloc(packed_buffer_size);
    std::memset(initial_data, 0, packed_buffer_size);
    std::memcpy(initial_data, initial_position.data(), position_ssbo_size);

    glGenBuffers(1, &packed_particles_buffer_handle);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, packed_particles_buffer_handle);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, packed_buffer_size, initial_data, GL_DYNAMIC_STORAGE_BIT);
    std::free(initial_data);

    glGenVertexArrays(1, &particle_position_vao_handle);
    glBindVertexArray(particle_position_vao_handle);

    glBindBuffer(GL_ARRAY_BUFFER, packed_particles_buffer_handle);
    // bind buffer containing particle position to vao, stride is 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    // enable attribute with binding = 0 (vertex position in the shader) for this vao
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);


    // bindings
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, packed_particles_buffer_handle, position_ssbo_offset, position_ssbo_size);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, packed_particles_buffer_handle, velocity_ssbo_offset, velocity_ssbo_size);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 2, packed_particles_buffer_handle, force_ssbo_offset, force_ssbo_size);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 3, packed_particles_buffer_handle, density_ssbo_offset, density_ssbo_size);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 4, packed_particles_buffer_handle, pressure_ssbo_offset, pressure_ssbo_size);

    glBindVertexArray(particle_position_vao_handle);

    // set clear color
    glClearColor(0.92f, 0.92f, 0.92f, 1.f);
    glEnable(GL_DEPTH_TEST);

    while (!glfwWindowShouldClose(window)) {
        static std::chrono::high_resolution_clock::time_point frame_start;
        static std::chrono::high_resolution_clock::time_point frame_end;
        static int64_t total_frame_time_ns;

        frame_start = std::chrono::high_resolution_clock::now();

        // process user inputs
        glfwPollEvents();

        // step through the simulation if not paused
        if (!paused)
        {
            glUseProgram(compute_program_handle[0]);
            glDispatchCompute(SPH_NUM_WORK_GROUPS, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            glUseProgram(compute_program_handle[1]);
            glDispatchCompute(SPH_NUM_WORK_GROUPS, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            glUseProgram(compute_program_handle[2]);
            glDispatchCompute(SPH_NUM_WORK_GROUPS, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            frame_number++;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(render_program_handle);
        glDrawArrays(GL_POINTS, 0, SPH_NUM_PARTICLES);

        glfwSwapBuffers(window);

        frame_end = std::chrono::high_resolution_clock::now();

        // measure performance
        total_frame_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_end - frame_start).count();

        std::stringstream title;
        title.precision(3);
        title.setf(std::ios_base::fixed, std::ios_base::floatfield);
        title << "SPH Simulation (OpenGL) | "
            "particle count: " << SPH_NUM_PARTICLES << " | "
            "frame " << frame_number << " | "
            "frame time: " << 1e-6 * total_frame_time_ns << " ms | ";
        glfwSetWindowTitle(window, title.str().c_str());
    }
    glDeleteProgram(render_program_handle);
    glDeleteProgram(compute_program_handle[0]);
    glDeleteProgram(compute_program_handle[1]);
    glDeleteProgram(compute_program_handle[2]);

    glDeleteVertexArrays(1, &particle_position_vao_handle);
    glDeleteBuffers(1, &packed_particles_buffer_handle);
    glfwTerminate();

    return 0;
}