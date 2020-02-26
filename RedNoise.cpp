#include <ModelTriangle.h>
#include <CanvasTriangle.h>
#include <DrawingWindow.h>
#include <Utils.h>
#include <glm/glm.hpp>
#include <fstream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <math.h>

#include "Texture.h"
#include "GObject.h"
#include "OBJ_IO.h"
#include "Camera.h"
#include "DepthBuffer.h"

using namespace std;
using namespace glm;

// Definitions
// ---
#define WIDTH 500
#define HEIGHT 500

Colour COLOURS[] = {Colour(255, 0, 0), Colour(0, 255, 0), Colour(0, 0, 255)};
Colour WHITE = Colour(255, 255, 255);
Colour BLACK = Colour(0, 0, 0);

typedef enum {WIRE, RASTER, RAY} View_mode;

// Global Object Instantiations
// ---
DrawingWindow window = DrawingWindow(WIDTH, HEIGHT, false);

Texture the_image("texture.ppm");
DepthBuffer depthbuf(WIDTH, HEIGHT);
Camera camera;
View_mode current_mode = RASTER;
OBJ_IO obj_io;
std::vector<GObject> gobjects = obj_io.loadOBJ("cornell-box.obj");

// Simple Helper Functions
// ---
float max(float A, float B) { if (A > B) return A; return B; }
int modulo(int x, int y) { return ((x % y) + x) % y; }
bool comparator(CanvasPoint p1, CanvasPoint p2) { return (p1.y < p2.y); }

uint32_t get_rgb(Colour colour) { return (0 << 24) + (colour.red << 16) + (colour.green << 8) + colour.blue; }
uint32_t get_rgb(vec3 rgb) { return (uint32_t)((255<<24) + (int(rgb[0])<<16) + (int(rgb[1])<<8) + int(rgb[2])); }

// Raster Functions
// ---
std::vector<double> interpolate(double from, double to, int numberOfValues) {
  double delta = (to - from) / (numberOfValues - 1);
  std::vector<double> v;
  for (int i = 0; i < numberOfValues; i++) v.push_back(from + (i * delta));
  //for (int i = 0; i < numberOfValues; i++) std::cout << v.at(i) << '\n';
  return v;
}

std::vector<CanvasPoint> interpolate_line(CanvasPoint from, CanvasPoint to) {
  float delta_X    = to.x - from.x;
  float delta_Y    = to.y - from.y;
  float delta_tp_X = to.texturePoint.x - from.texturePoint.x;
  float delta_tp_Y = to.texturePoint.y - from.texturePoint.y;
  double delta_dep = to.depth - from.depth;

  float no_steps = max(abs(delta_X), abs(delta_Y));

  float stepSize_X    = delta_X / no_steps;
  float stepSize_Y    = delta_Y / no_steps;
  float stepSize_tp_X = delta_tp_X / no_steps;
  float stepSize_tp_Y = delta_tp_Y / no_steps;
  double stepSize_dep = delta_dep / no_steps;
  //std::cout << "From depth: " << from.depth << ", To depth: " << to.depth << '\n';
  std::vector<CanvasPoint> v;
  for (float i = 0.0; i < no_steps; i++) {
    CanvasPoint interp_point(from.x + (i * stepSize_X), from.y + (i * stepSize_Y), from.depth + (i * stepSize_dep));
    //std::cout << "Point " << i << ": " << interp_point << '\n';
    TexturePoint interp_tp(from.texturePoint.x + (i * stepSize_tp_X), from.texturePoint.y + (i * stepSize_tp_Y));
    interp_point.texturePoint = interp_tp;
    v.push_back(interp_point);
  }
  return v;
}

void drawLine(CanvasPoint P1, CanvasPoint P2, Colour colour) {
  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    if (depthbuf.update(pixel)) {
      window.setPixelColour(round(x), round(y), get_rgb(colour));
    }
    //window.setPixelColour(round(x), round(y), get_rgb(colour));
  }
}

uint32_t get_textured_pixel(TexturePoint texturePoint) {
  return the_image.ppm_image[(int)(round(texturePoint.x) + (round(texturePoint.y) * the_image.width))];
}

void drawTexturedLine(CanvasPoint P1, CanvasPoint P2) {
  std::vector<CanvasPoint> interp_line = interpolate_line(P1, P2);

  for (uint i = 0; i < interp_line.size(); i++) {
    CanvasPoint pixel = interp_line.at(i);
    float x = pixel.x;
    float y = pixel.y;
    uint32_t colour = get_textured_pixel(pixel.texturePoint);
    window.setPixelColour(round(x), round(y), colour);
  }
}

void drawStrokedTriangle(CanvasTriangle triangle) {
  drawLine(triangle.vertices[0], triangle.vertices[1], triangle.colour);
  drawLine(triangle.vertices[0], triangle.vertices[2], triangle.colour);
  drawLine(triangle.vertices[1], triangle.vertices[2], triangle.colour);
}

void sortTrianglePoints(CanvasTriangle *triangle) {
  std::sort(std::begin(triangle->vertices), std::end(triangle->vertices), comparator);
  //std::cout << "SORTED: " << triangle->vertices[0].y << " " << triangle->vertices[1].y << " " << triangle->vertices[2].y << '\n';
}

void equateTriangles(CanvasTriangle from, CanvasTriangle *to) {
  for (int i = 0; i < 3; i++) {
    to->vertices[i].x = from.vertices[i].x;
    to->vertices[i].y = from.vertices[i].y;
    to->vertices[i].depth = from.vertices[i].depth;
    to->vertices[i].texturePoint = from.vertices[i].texturePoint;
  }
  to->colour = from.colour;
}

void getTopBottomTriangles(CanvasTriangle triangle, CanvasTriangle *top, CanvasTriangle *bottom) {
  if ((round(triangle.vertices[0].y) == round(triangle.vertices[1].y)) || (round(triangle.vertices[1].y) == round(triangle.vertices[2].y))) {
    //std::cout << "detected a flat triangle!" << '\n';
    CanvasPoint temp = triangle.vertices[1];
    triangle.vertices[1] = triangle.vertices[2];
    triangle.vertices[2] = temp;
    equateTriangles(triangle, top);
    equateTriangles(triangle, bottom);
    return;
  }

  CanvasPoint point4;
  std::vector<CanvasPoint> v = interpolate_line(triangle.vertices[0], triangle.vertices[2]);
  for (float i = 0.0; i < v.size(); i++) {
    if (round(v.at(i).y) == round(triangle.vertices[1].y)) {
      point4 = v.at(i);
    }
  }

  //CONSTRUCTOR: CanvasTriangle(CanvasPoint v0, CanvasPoint v1, CanvasPoint v2, Colour c)
  CanvasTriangle top_Triangle(triangle.vertices[0], point4, triangle.vertices[1], triangle.colour);
  CanvasTriangle bot_Triangle(point4, triangle.vertices[2], triangle.vertices[1], triangle.colour);

  equateTriangles(top_Triangle, top);
  equateTriangles(bot_Triangle, bottom);
}

void fillFlatBaseTriangle(CanvasTriangle triangle, int side1_A, int side1_B, int side2_A, int side2_B, bool textured) {
  //std::cout << "drawing filled triangle: " << triangle << '\n';
  std::vector<CanvasPoint> side1 = interpolate_line(triangle.vertices[side1_A], triangle.vertices[side1_B]);
  std::vector<CanvasPoint> side2 = interpolate_line(triangle.vertices[side2_A], triangle.vertices[side2_B]);

  //std::cout << "side1.size() " << side1.size() << " side2.size() " << side2.size() << "\n";
  uint last_drawn_y = round(side1.at(0).y);

  if (textured) drawTexturedLine(side1.at(0), side2.at(0));
  else drawLine(side1.at(0), side2.at(0), triangle.colour);

  for (uint i = 0; i < side1.size(); i++) {
    if (round(side1.at(i).y) != last_drawn_y) {
      int j = 0;
      while ((j < (int)side2.size() - 1) && (round(side2.at(j).y) <= last_drawn_y)) {
	       j++;
      }
      //std::cout << "i " << round(side1.at(i).y) << " j " << round(side2.at(j).y);
      if (textured) drawTexturedLine(side1.at(i), side2.at(j));
      else {
        //std::cout << "Side1: " << side1.at(i).depth << " Side2: " << side2.at(j).depth << '\n';
        drawLine(side1.at(i), side2.at(j), triangle.colour);
      }
      //std::cout << " DRAWN" << '\n';
      last_drawn_y++;
    }
  }
}

bool triangleIsLine(CanvasTriangle triangle) {
  std::vector<CanvasPoint> v = interpolate_line(triangle.vertices[0], triangle.vertices[1]);
  for (uint i = 0; i < v.size(); i++) {
    if ((round(v.at(i).x) == round(triangle.vertices[2].x)) && (round(v.at(i).y) == round(triangle.vertices[2].y))) {
      return true;
    }
  }

  v = interpolate_line(triangle.vertices[0], triangle.vertices[2]);
  for (uint i = 0; i < v.size(); i++) {
    if ((round(v.at(i).x) == round(triangle.vertices[1].x)) && (round(v.at(i).y) == round(triangle.vertices[1].y))) {
      return true;
    }
  }

  v = interpolate_line(triangle.vertices[1], triangle.vertices[2]);
  for (uint i = 0; i < v.size(); i++) {
    if ((round(v.at(i).x) == round(triangle.vertices[0].x)) && (round(v.at(i).y) == round(triangle.vertices[0].y))) {
      return true;
    }
  }

  return false;
}

void drawFilledTriangle(CanvasTriangle triangle, bool textured) {
  sortTrianglePoints(&triangle);
  CanvasTriangle triangles[2];

  if (triangleIsLine(triangle)) {
    drawLine(triangle.vertices[0], triangle.vertices[1], triangle.colour);
    drawLine(triangle.vertices[1], triangle.vertices[2], triangle.colour);
  }
  else {
    getTopBottomTriangles(triangle, &triangles[0], &triangles[1]);
    fillFlatBaseTriangle(triangles[0], 0, 1, 0, 2, textured);
    fillFlatBaseTriangle(triangles[1], 0, 1, 2, 1, textured);
  }
}

glm::vec3 getAdjustedVector(glm::vec3 v) {
  glm::vec3 cam2vertex = v - camera.position;
  return cam2vertex * camera.orientation;
}

CanvasPoint projectVertexInto2D(glm::vec3 v) {
  // Vector from camera to verted adjusted w.r.t. to the camera's orientation.
  glm::vec3 adjVec = getAdjustedVector(v);

  double w_i;                      //  width of canvaspoint from camera axis
  double h_i;                      // height of canvaspoint from camera axis
  double d_i = camera.focalLength; // distance from camera to axis extension of canvas

  double depth = sqrt((adjVec.z * adjVec.z) + (adjVec.x * adjVec.x) + (adjVec.y * adjVec.y));
  // std::cout << "depth: " << depth << '\n';

  w_i = (adjVec.x * d_i) / adjVec.z;
  h_i = (adjVec.y * d_i) / adjVec.z;

  // NOTE: this should not actually be negative
  double x_factor = -WIDTH / 4;
  double y_factor = HEIGHT / 4;

  w_i = w_i * x_factor + (WIDTH / 2);
  h_i = h_i * y_factor + (HEIGHT / 2);

  CanvasPoint res((float)w_i, (float)h_i, depth);
  return res;
}

CanvasTriangle projectTriangleOntoImagePlane(ModelTriangle triangle) {
  CanvasTriangle result;
  //generateTriangle(&result);
  for (int i = 0; i < 3; i++) {
    result.vertices[i] = projectVertexInto2D(triangle.vertices[i]);
    //std::cout << "vertex at " << i << ": " << result.vertices[i] << '\n';
  }
  result.colour = triangle.colour;

  return result;
}

void drawGeometry(std::vector<GObject> gobjs) {
  for (uint i = 0; i < gobjs.size(); i++) {
    for (uint j = 0; j < gobjs.at(i).faces.size(); j++) {
      //std::cout << "i: " << i << " , j: " << j << '\n';
      //std::cout << "object: " << gobjs.at(i).name << '\n';
      CanvasTriangle projectedTriangle = projectTriangleOntoImagePlane(gobjs.at(i).faces.at(j));
      drawFilledTriangle(projectedTriangle, false);
    }
  }
}

void drawGeometryWireFrame(std::vector<GObject> gobjs) {
  for (uint i = 0; i < gobjs.size(); i++) {
    for (uint j = 0; j < gobjs.at(i).faces.size(); j++) {
      CanvasTriangle projectedTriangle = projectTriangleOntoImagePlane(gobjs.at(i).faces.at(j));
      drawStrokedTriangle(projectedTriangle);
    }
  }
}

void clearScreen() {
  window.clearPixels();

  for(int y=0; y<window.height ;y++) {
    for(int x=0; x<window.width ;x++) {
      window.setPixelColour(x, y, get_rgb(WHITE));
    }
  }

  depthbuf.clear();
}

void draw() {
  clearScreen();
  if (current_mode == WIRE) {
    drawGeometryWireFrame(gobjects);
  }
  else if (current_mode == RASTER) {
    drawGeometry(gobjects);
  }
  else {
    std::cout << "Raytracing not yet supported\n";
    exit(1);
  }
  camera.printCamera();
}

void handleEvent(SDL_Event event) {
  glm::vec3 prev_pos;
  if(event.type == SDL_KEYDOWN) {
    if(event.key.keysym.sym == SDLK_LEFT) cout << "LEFT" << endl;
    else if(event.key.keysym.sym == SDLK_RIGHT) cout << "RIGHT" << endl;
    else if(event.key.keysym.sym == SDLK_c) {
      cout << "C: CLEARING SCREEN" << endl;
      clearScreen();
    }
    else if(event.key.keysym.sym == SDLK_b) {
      cout << "B: DRAW CORNELL BOX (RASTER)" << endl;
      current_mode = RASTER;
      draw();
    }
    else if(event.key.keysym.sym == SDLK_f) {
      cout << "F: DRAW WIREFRAME" << endl;
      current_mode = WIRE;
      draw();
    }
    else if(event.key.keysym.sym == SDLK_w) {
      cout << "W: MOVE CAMERA FORWARD" << endl;
      camera.moveBy(0, 0, -1);
      draw();
    }
    else if(event.key.keysym.sym == SDLK_a) {
      cout << "A: MOVE CAMERA LEFT" << endl;
      camera.moveBy(-1, 0, 0);
      draw();
    }
    else if(event.key.keysym.sym == SDLK_s) {
      cout << "S: MOVE CAMERA BACKWARD" << endl;
      camera.moveBy(0, 0, 1);
      draw();
    }
    else if(event.key.keysym.sym == SDLK_d) {
      cout << "D: MOVE CAMERA RIGHT" << endl;
      camera.moveBy(1, 0, 0);
      draw();
    }
    else if(event.key.keysym.sym == SDLK_UP) {
      cout << "UP: MOVE CAMERA UP" << endl;
      camera.moveBy(0, 1, 0);
      draw();
    }
    else if(event.key.keysym.sym == SDLK_DOWN) {
      cout << "DOWN: MOVE CAMERA DOWN" << endl;
      camera.moveBy(0, -1, 0);
      draw();
    }
    else if(event.key.keysym.sym == SDLK_q) {
      cout << "Q: ROTATE CAMERA ANTICLOCKWISE Y AXIS" << endl;
      camera.rotateBy(1.0);
      draw();
    }
    else if(event.key.keysym.sym == SDLK_e) {
      cout << "Q: ROTATE CAMERA CLOCKWISE ABOUT Y AXIS" << endl;
      camera.rotateBy(-1.0);
      draw();
    }
  }
  else if(event.type == SDL_MOUSEBUTTONDOWN) cout << "MOUSE CLICKED" << endl;
}

int main(int argc, char* argv[]) {
  srand((unsigned) time(0));
  SDL_Event event;
  // camera.orientation = camera.orientation * rotMatY(deg2rad(180.0));
  draw();
  camera.printCamera();

  while(true) {
    // We MUST poll for events - otherwise the window will freeze !
    if(window.pollForInputEvents(&event)) handleEvent(event);

    // Need to render the frame at the end, or nothing actually gets shown on the screen !
    window.renderFrame();
  }
}
