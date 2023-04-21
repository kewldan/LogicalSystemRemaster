#pragma once

#define logLoadedTexture(name, w, h) PLOGI << "Texture [" << name << "] loaded (" << w << "x" << h << ")"

#ifndef NDEBUG
#define glObjectLabelStr(type, name, label) glObjectLabel(type, name, (int) strlen(label), label)
#define glObjectLabelBuild(type, name, type_name, description) {char* _ = new char[256]; strcpy_s(_, 256, type_name); strcat_s(_, 256, " ["); strcat_s(_, 256, description); strcat_s(_, 256, "]"); glObjectLabelStr(type, name, _); delete[] _;}
#else
#define glObjectLabelStr(type, name, label)
#define glObjectLabelBuild(type, name, type_name, description)
#endif
