/* Host test of the Switch Mesa patches through the GL API (OSMesa, softpipe): the GLSL cache and
 * program binaries on the single-file disk cache. Run twice on the same MESA_SHADER_CACHE_DIR:
 *   test_glsl_cache store   compiles and links the programs, saves their binaries and pixels
 *   test_glsl_cache load    compiles and links again (MESA_GLSL=cache_info shows the cache
 *                           hits), loads the saved binaries, and checks that every program,
 *                           linked from the cache or loaded from a binary, draws the same pixels
 * Prints "GLSL CACHE OK" when every check passes. */
#define GL_GLEXT_PROTOTYPES 1
#include <GL/osmesa.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 32
#define H 32
#define PROGRAMS 6

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static const char *vs =
   "#version 330\n"
   "layout(location = 0) in vec2 pos;\n"
   "out vec2 uv;\n"
   "uniform vec4 xform;\n"
   "void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos * xform.xy + xform.zw, 0.0, 1.0); }\n";
static const char *fs_fmt =
   "#version 330\n"
   "in vec2 uv;\n"
   "out vec4 color;\n"
   "uniform vec4 tint;\n"
   "void main() {\n"
   "  vec3 c = vec3(uv, 0.%d);\n"
   "  for (int i = 0; i < %d; i++) c = fract(c * 1.7 + tint.rgb);\n"
   "  color = vec4(c, 1.0);\n"
   "}\n";

static GLuint compile(GLenum type, const char *src)
{
   GLuint s = glCreateShader(type);
   glShaderSource(s, 1, &src, NULL);
   glCompileShader(s);
   GLint ok = 0;
   glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
   CHECK(ok);
   return s;
}

static void draw(GLuint prog, unsigned char *pixels)
{
   glUseProgram(prog);
   glUniform4f(glGetUniformLocation(prog, "xform"), 1, 1, 0, 0);
   glUniform4f(glGetUniformLocation(prog, "tint"), 0.1f, 0.2f, 0.3f, 1);
   glClearColor(0, 0, 0, 0);
   glClear(GL_COLOR_BUFFER_BIT);
   glDrawArrays(GL_TRIANGLES, 0, 3);
   glFinish();
   glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

int main(int argc, char **argv)
{
   const int store = argc > 1 && strcmp(argv[1], "store") == 0;
   const char *dir = getenv("MESA_SHADER_CACHE_DIR");
   const int attribs[] = {OSMESA_FORMAT, OSMESA_RGBA, OSMESA_PROFILE, OSMESA_CORE_PROFILE,
                          OSMESA_CONTEXT_MAJOR_VERSION, 3, OSMESA_CONTEXT_MINOR_VERSION, 3, 0};
   OSMesaContext ctx = OSMesaCreateContextAttribs(attribs, NULL);
   static unsigned char buffer[W * H * 4];
   CHECK(ctx && OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, W, H));
   GLint formats = 0;
   glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);
   printf("GL_NUM_PROGRAM_BINARY_FORMATS %d\n", formats);
   CHECK(formats == 1);

   GLuint vao, vbo;
   const float tri[] = {-1, -1, 3, -1, -1, 3};
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
   glEnableVertexAttribArray(0);

   for (int p = 0; p < PROGRAMS; p++) {
      char fs[1024];
      snprintf(fs, sizeof(fs), fs_fmt, p + 1, 4 + p);
      GLuint prog = glCreateProgram();
      GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
      glAttachShader(prog, v);
      glAttachShader(prog, f);
      glLinkProgram(prog);
      GLint ok = 0;
      glGetProgramiv(prog, GL_LINK_STATUS, &ok);
      CHECK(ok);
      unsigned char pixels[W * H * 4], expect[W * H * 4];
      draw(prog, pixels);
      char path[512];
      snprintf(path, sizeof(path), "%s/test_program_%d.bin", dir, p);
      if (store) {
         GLint length = 0;
         glGetProgramiv(prog, GL_PROGRAM_BINARY_LENGTH, &length);
         CHECK(length > 0);
         void *binary = malloc(length);
         GLenum format = 0;
         glGetProgramBinary(prog, length, &length, &format, binary);
         FILE *out = fopen(path, "wb");
         fwrite(&format, sizeof(format), 1, out);
         fwrite(pixels, 1, sizeof(pixels), out);
         fwrite(binary, 1, length, out);
         fclose(out);
         free(binary);
         printf("program %d: binary %d bytes, pixel (16,16) = %u %u %u\n", p, length,
                pixels[(16 * W + 16) * 4], pixels[(16 * W + 16) * 4 + 1], pixels[(16 * W + 16) * 4 + 2]);
      } else {
         FILE *in = fopen(path, "rb");
         CHECK(in != NULL);
         if (!in)
            continue;
         GLenum format = 0;
         fread(&format, sizeof(format), 1, in);
         fread(expect, 1, sizeof(expect), in);
         static unsigned char binary[1 << 20];
         const size_t length = fread(binary, 1, sizeof(binary), in);
         fclose(in);
         CHECK(memcmp(pixels, expect, sizeof(pixels)) == 0);   /* linked from the GLSL cache */
         GLuint loaded = glCreateProgram();
         glProgramBinary(loaded, format, binary, (GLsizei)length);
         glGetProgramiv(loaded, GL_LINK_STATUS, &ok);
         CHECK(ok);
         draw(loaded, pixels);
         CHECK(memcmp(pixels, expect, sizeof(pixels)) == 0);   /* loaded from the binary */
         /* A damaged binary is refused, not used. */
         binary[length / 2] ^= 0xff;
         GLuint bad = glCreateProgram();
         glProgramBinary(bad, format, binary, (GLsizei)length);
         glGetProgramiv(bad, GL_LINK_STATUS, &ok);
         CHECK(!ok);
         while (glGetError() != GL_NO_ERROR) {
         }
      }
   }
   if (failures)
      printf("GLSL CACHE FAILED (%d)\n", failures);
   else
      printf("GLSL CACHE OK (%s)\n", store ? "store" : "load");
   return failures != 0;
}
