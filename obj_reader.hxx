#ifndef OBJ_READER
#define OBJ_READER

#include <string>
#include <vector>
#include <array>

struct ObjFaceVert {
  int vertex;
  int normal;
  int texcoord;
};

struct ObjFace {
  std::vector<ObjFaceVert> verts;
};

struct ObjObject {
  std::string name;
  std::vector<ObjFace> faces;
};

class ObjReader {
public:
  ObjReader(const std::string &data);

public:
  std::vector<ObjObject> objects;
  std::vector<std::array<float, 4>> vertex;
  std::vector<std::array<float, 3>> normal;
  std::vector<std::array<float, 2>> texcoord;
};

#endif // OBJ_READER
