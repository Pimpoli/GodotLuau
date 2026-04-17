#ifndef FOLDER_H
#define FOLDER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

class Folder : public Node {
    GDCLASS(Folder, Node);

protected:
    static void _bind_methods() {}

public:
    Folder() {}
    ~Folder() {}
};

#endif