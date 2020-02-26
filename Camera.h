#include <glm/glm.hpp>
#include <string>

using namespace glm;
using namespace std;

class Camera {
  public:
    vec3 position = vec3(0.0, 2.0, 10.0);
    //mat3 orientation = mat3(1.0F);
    mat3 orientation = mat3(-1, 0, 0,    // right
			    0, -1, 0,    // up
			    0, 0, -1);  // forward
    float focalLength = position[2] / 2;

    Camera () {}

    void moveBy(float x, float y, float z) {
      vec3 translationVector(x, y, z);
      position += translationVector;
    }

    void rotateBy(float deg) {
      orientation = rotMatY(deg2rad(deg)) * orientation;
    }

    void printCamera() {
      printCameraPosition();
      printCameraOrientation();
    }

  private:
    float deg2rad(float deg) { return (deg * M_PI) / 180; }

    glm::mat3 rotMatX(float angle) { return mat3(1,0,0, 0,cos(angle),-sin(angle), 0,sin(angle),cos(angle)); }
    glm::mat3 rotMatY(float angle) { return mat3(cos(angle),0,sin(angle), 0,1,0, -sin(angle),0,cos(angle)); }
    glm::mat3 rotMatZ(float angle) { return mat3(cos(angle),-sin(angle),0, sin(angle),cos(angle),0, 0,0,1); }

    void printVec3(vec3 v) {
      cout << "(" << v.x << ", " << v.y << ", " << v.z << ")\n";
    }

    void printCameraPosition() {
      cout << "CAMERA position: \n";
      printVec3(position);
    }

    void printCameraOrientation() {
      cout << "CAMERA orientation:\n";
      for (int i = 0; i < 3; i++) printVec3(orientation[i]);
    }

};
