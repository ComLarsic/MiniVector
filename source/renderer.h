#pragma once

#include "renderer.h"
#include "shader.h"
#include "display.h"
#include "color.h"

#include <glad/glad.h>

#define MV_LINE_VERTEX_SHADER                                         \
    "#version 460 core\n"                                             \
    "layout (location = 0) in vec2 aPos;\n"                           \
    "uniform mat4 projection;\n"                                      \
    "uniform vec3 color;\n"                                           \
    "out vec3 vertexColor;\n"                                         \
    "void main()\n"                                                   \
    "{\n"                                                             \
    "   gl_Position = projection * vec4(aPos.x, aPos.y, 0.0, 1.0);\n" \
    "   vertexColor = color;\n"                                       \
    "}\n"

#define MV_LINE_FRAGMENT_SHADER                \
    "#version 460 core\n"                      \
    "out vec4 FragColor;\n"                    \
    "in vec3 vertexColor;\n"                   \
    "void main()\n"                            \
    "{\n"                                      \
    "   FragColor = vec4(vertexColor, 1.0);\n" \
    "}\n"

/**
 * @brief The line renderer
 */
typedef struct mv_renderer
{
    GLuint vao;
    GLuint vbo;
    mv_shader *shader;
} mv_renderer;

/**
 * @brief Create the renderer
 */
mv_renderer *mv_create_renderer();

/**
 * @brief Set the renderer projection. The projection is orthographic
 *
 * @param renderer The renderer
 * @param width The width of the projection
 * @param height The height of the projection
 */
void mv_renderer_set_projection(mv_renderer *renderer, float width, float height);

/**
 * @brief Draw the display
 *
 * @param renderer The renderer
 * @param display The display to draw
 * @param primary The color of the background
 * @param secondary The color of the lines
 */
void mv_renderer_draw(mv_renderer *renderer, mv_display *display, mv_color primary, mv_color secondary);

/**
 * @brief Destroy the renderer
 *
 * @param renderer The renderer to destroy
 */
void mv_destroy_renderer(mv_renderer *renderer);

/**
 * @brief Create the buffers for the renderer
 */
static void create_buffers(mv_renderer *renderer);

/**
 * @brief Create the orthographic projection
 * @param left The left side of the projection
 * @param right The right side of the projection
 * @param bottom The bottom side of the projection
 * @param top The top side of the projection
 * @param near The near side of the projection
 * @param far The far side of the projection
 * @return The orthographic projection
 */
static float *orthographic(float left, float right, float bottom, float top, float near, float far);