#include "obj_reader.hxx"

#include <sstream>

std::string get_line(const std::string &data, int &offset) {
  std::ostringstream ss;

  bool in_comment = false;

  for (; offset < data.size(); ++offset) {
    if (data[offset] == '\n') {
      offset++;
      break;
    } else if (data[offset] == '\r' && (offset + 1) < data.size() &&
               data[offset + 1] == '\n') {
      offset += 2;
      break;
    } else if (!in_comment && data[offset] == '#') {
      in_comment = true;
    } else if (!in_comment) {
      ss << data[offset];
    }
  }

  return ss.str();
}

std::vector<std::string> get_words(const std::string &line, char sep = ' ') {
  std::vector<std::string> words;
  std::string current_word = "";
  for (size_t i = 0; i < line.size(); ++i) {
    if (line[i] == sep) {
      words.push_back(current_word);
      current_word = "";
    } else {
      current_word += line[i];
    }
  }

  if (current_word.length() > 0u) {
    words.push_back(current_word);
  }

  return words;
}

ObjReader::ObjReader(const std::string &data) {
  int offset = 0;
  std::string line;
  ObjObject *curr_object = nullptr;

  while (offset < data.size()) {
    line = get_line(data, offset);
    std::vector<std::string> words = get_words(line);
    if (words.size() == 0u) {
      continue;
    }

    const std::string &cmd = words[0];

    if (cmd == "o") {
      ObjObject obj;
      obj.name = words[1];
      objects.push_back(obj);
      curr_object = &objects[objects.size() - 1u];

    } else if (cmd == "v") {
      std::array<float, 4> v = { 0.0f, 0.0f, 0.0f, 1.0f };
      for (int i = 0; i < 4 && i < words.size() - 1; ++i) {
        v[i] = atof(words[i + 1].c_str());
      }
      vertex.push_back(v);

    } else if (cmd == "vn") {
      std::array<float, 3> n = {0.0f, 0.0f, 0.0f};
      for (int i = 0; i < 3 && i < words.size() - 1; ++i) {
        n[i] = atof(words[i + 1].c_str());
      }
      normal.push_back(n);

    } else if (cmd == "vt") {
      std::array<float, 2> t = {0.0f, 0.0f};
      for (int i = 0; i < 2 && i < words.size() - 1; ++i) {
        t[i] = atof(words[i + 1].c_str());
      }
      texcoord.push_back(t);

    } else if (cmd == "f") {
      ObjFace f;
      for (size_t i = 1; i < words.size(); ++i) {
        std::vector<std::string> indices_str = get_words(words[i], '/');
        int pos_index = atoi(indices_str[0].c_str()) - 1;
        int tex_index = -1;
        int norm_index = -1;
        if (indices_str[1].length() > 0u) {
          tex_index = atoi(indices_str[1].c_str()) - 1;
        }
        if (indices_str[2].length() > 0u) {
          norm_index = atoi(indices_str[2].c_str()) - 1;
        }
        ObjFaceVert fv;
        fv.vertex = pos_index;
        fv.texcoord = tex_index;
        fv.normal = norm_index;
        f.verts.push_back(fv);
      }
      curr_object->faces.push_back(f);
    }
  }
}
