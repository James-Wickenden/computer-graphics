
using namespace std;
using namespace glm;

class TextureTriangle {
  vec2 vertices[3];
  string textureFilename;

  public:
    TextureTriangle () {}

    TextureTriangle (string filename, vec2 a, vec2 b, vec2 c) {
      textureFilename = filename;
      vertices[0] = a;
      vertices[1] = b;
      vertices[2] = c;
    }

};
