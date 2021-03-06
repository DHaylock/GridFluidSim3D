/*
Copyright (c) 2016 Ryan L. Guy

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgement in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
#include "trianglemesh.h"

TriangleMesh::TriangleMesh() {
}

TriangleMesh::~TriangleMesh() {
}

int TriangleMesh::numVertices() {
    return (int)vertices.size();
}

int TriangleMesh::numFaces() {
    return (int)triangles.size();
}

void TriangleMesh::clear() {
    vertices.clear();
    normals.clear();
    triangles.clear();
    vertexcolors.clear();
    _vertexTriangles.clear();
}

// method of loading OBJ from:
// http://www.opengl-tutorial.org/beginners-tutorials/tutorial-7-model-loading/
// .obj must be a closed watertight mesh with triangle with either shared triangle
// vertices in correct winding order, or vertices with pre-computed vertex normals.
bool TriangleMesh::loadOBJ(std::string filename, vmath::vec3 offset, double scale) {
    clear();

    std::vector<vmath::vec3> temp_vertices;
    std::vector<vmath::vec3> temp_normals;
    std::vector<Triangle> temp_triangles;

    FILE * file;
    file = fopen(filename.c_str(), "rb");
    if( file == nullptr ){
        printf("Unable to open the OBJ file!\n");
        return false;
    }

    while( true ){
        char lineHeader[128];
        // read the first word of the line
        int res = fscanf(file, "%s", lineHeader);
        if (res == EOF) {
            break; // EOF = End Of File. Quit the loop.
        }
        
        if ( strcmp( lineHeader, "v" ) == 0 ){
            vmath::vec3 vertex;
            fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z );
            temp_vertices.push_back((float)scale*vertex + offset);
        } else if (strcmp( lineHeader, "vn" ) == 0) {
            vmath::vec3 normal;
            fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z );
            temp_normals.push_back(normal);
        } else if ( strcmp( lineHeader, "f" ) == 0 ) {
            long start = ftell(file);
            unsigned int vertexIndex[3];
            unsigned int uvIndex[3];
            unsigned int normalIndex[3];
            int matches = fscanf(file, "%d %d %d\n", &vertexIndex[0], &vertexIndex[1], &vertexIndex[2]);

            if (matches != 3){
                long diff = ftell(file) - start;
                fseek (file, -diff , SEEK_CUR);
                start = ftell(file);
                matches = fscanf(file, "%d//%d %d//%d %d//%d\n", &vertexIndex[0], &normalIndex[0], 
                                                                 &vertexIndex[1], &normalIndex[1],
                                                                 &vertexIndex[2], &normalIndex[2]);
                if (matches != 6) {
                    long diff = ftell(file) - start;
                    fseek (file, -diff , SEEK_CUR);
                    matches = fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", 
                                              &vertexIndex[0], &normalIndex[0], &uvIndex[0],
                                              &vertexIndex[1], &normalIndex[1], &uvIndex[1],
                                              &vertexIndex[2], &normalIndex[2], &uvIndex[2]);

                    if (matches != 9) {
                        printf("File can't be read by our simple parser : ( Try exporting with other options\n");
                        return false;
                    }
                }
            }

            Triangle t = Triangle(vertexIndex[0] - 1,
                                  vertexIndex[1] - 1,
                                  vertexIndex[2] - 1);
            temp_triangles.push_back(t);
        }
    }

    fclose(file);

    vertices.insert(vertices.end(), temp_vertices.begin(), temp_vertices.end());
    triangles.insert(triangles.end(), temp_triangles.begin(), temp_triangles.end());
    removeDuplicateTriangles();

    if (normals.size() == vertices.size()) {
        normals.clear();
        normals.insert(normals.end(), normals.begin(), normals.end());
    } else {
        updateVertexNormals();
    }

    return true;
}

bool TriangleMesh::loadPLY(std::string PLYFilename) {
    clear();

    std::ifstream file(PLYFilename.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::string header;
    bool success = _getPLYHeader(&file, &header);
    if (!success) {
        return false;
    }

    success = _loadPLYVertexData(&file, header);
    if (!success) {
        return false;
    }

    success = _loadPLYTriangleData(&file, header);
    if (!success) {
        return false;
    }

    return true;
}

void TriangleMesh::writeMeshToOBJ(std::string filename) {
    assert(normals.size() == vertices.size());

    std::ostringstream str;

    str << "# OBJ file format with ext .obj" << std::endl;
    str << "# vertex count = " << vertices.size() << std::endl;
    str << "# face count = " << triangles.size() << std::endl;

    vmath::vec3 p;
    for (unsigned int i = 0; i < vertices.size(); i++) {
        p = vertices[i];
        str << "v " << p.x << " " << p.y << " " << p.z << std::endl;
    }

    vmath::vec3 n;
    for (unsigned int i = 0; i < normals.size(); i++) {
        n = normals[i];
        str << "vn " << n.x << " " << n.y << " " << n.z << std::endl;
    }

    Triangle t;
    int v1, v2, v3;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        v1 = t.tri[0] + 1;
        v2 = t.tri[1] + 1;
        v3 = t.tri[2] + 1;

        str << "f " << v1 << "//" << v1 << " " <<
            v2 << "//" << v2 << " " <<
            v3 << "//" << v3 << std::endl;
    }

    std::ofstream out(filename);
    out << str.str();
    out.close();
}

void TriangleMesh::writeMeshToSTL(std::string filename) {

    // 80 char header, 4 byte num triangles, 50 bytes per triangle
    int binsize = 80*sizeof(char) + sizeof(unsigned int) + 
                  triangles.size() * (12*sizeof(float) + sizeof(unsigned short));
    char *bin = new char[binsize];

    for (int i = 0; i < binsize; i++) {
        bin[i] = 0x00;
    }

    int offset = 80;
    unsigned int numTriangles = triangles.size();
    memcpy(bin + offset, &numTriangles, sizeof(unsigned int));
    offset += sizeof(int);

    float tri[12*sizeof(float)];
    Triangle t;
    vmath::vec3 normal, v1, v2, v3;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        normal = getTriangleNormal(i);
        v1 = vertices[t.tri[0]];
        v2 = vertices[t.tri[1]];
        v3 = vertices[t.tri[2]];

        tri[0] = normal.x;
        tri[1] = normal.y;
        tri[2] = normal.z;
        tri[3] = v1.x;
        tri[4] = v1.y;
        tri[5] = v1.z;
        tri[6] = v2.x;
        tri[7] = v2.y;
        tri[8] = v2.z;
        tri[9] = v3.x;
        tri[10] = v3.y;
        tri[11] = v3.z;

        memcpy(bin + offset, tri, 12*sizeof(float));
        offset += 12*sizeof(float) + sizeof(unsigned short);
    }

    std::ofstream erasefile;
    erasefile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    erasefile.close();

    std::ofstream file(filename.c_str(), std::ios::out | std::ios::binary);
    file.write(bin, binsize);
    file.close();

    delete[] bin;
}

void TriangleMesh::writeMeshToPLY(std::string filename) {
    // Header format:
    /*
        ply
        format binary_little_endian 1.0
        element vertex FILL_IN_NUMBER_OF_VERTICES
        property float x
        property float y
        property float z
        element face FILL_IN_NUMBER_OF_FACES
        property list uchar int vertex_index
        end_header
    */
    
    char header1[51] = {'p', 'l', 'y', '\n', 
                        'f', 'o', 'r', 'm', 'a', 't', ' ', 'b', 'i', 'n', 'a', 'r', 'y', '_', 'l', 
                        'i', 't', 't', 'l', 'e', '_', 'e', 'n', 'd', 'i', 'a', 'n', ' ', '1', '.', '0', '\n',
                        'e', 'l', 'e', 'm', 'e', 'n', 't', ' ', 'v', 'e', 'r', 't', 'e', 'x', ' '};
                    
    char header2[65] = {'\n', 'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'f', 'l', 'o', 'a', 't', ' ', 'x', '\n',
                              'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'f', 'l', 'o', 'a', 't', ' ', 'y', '\n',
                              'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'f', 'l', 'o', 'a', 't', ' ', 'z', '\n',
                              'e', 'l', 'e', 'm', 'e', 'n', 't', ' ', 'f', 'a', 'c', 'e', ' '};

    char header2color[125] = {'\n', 'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'f', 'l', 'o', 'a', 't', ' ', 'x', '\n',
                                    'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'f', 'l', 'o', 'a', 't', ' ', 'y', '\n',
                                    'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'f', 'l', 'o', 'a', 't', ' ', 'z', '\n',
                                    'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'u', 'c', 'h', 'a', 'r', ' ', 'r', 'e', 'd', '\n',
                                    'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'u', 'c', 'h', 'a', 'r', ' ', 'g', 'r', 'e', 'e', 'n', '\n',
                                    'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'u', 'c', 'h', 'a', 'r', ' ', 'b', 'l', 'u', 'e', '\n',
                                    'e', 'l', 'e', 'm', 'e', 'n', 't', ' ', 'f', 'a', 'c', 'e', ' '};
                          
    char header3[49] = {'\n', 'p', 'r', 'o', 'p', 'e', 'r', 't', 'y', ' ', 'l', 'i', 's', 't', ' ', 
                              'u', 'c', 'h', 'a', 'r', ' ', 'i', 'n', 't', ' ', 
                              'v', 'e', 'r', 't', 'e', 'x', '_', 'i', 'n', 'd', 'e', 'x', '\n',
                              'e', 'n', 'd', '_', 'h', 'e', 'a', 'd', 'e', 'r', '\n'};

    bool isColorEnabled = vertices.size() == vertexcolors.size();

    char vertstring[10];
    char facestring[10];
    int vertdigits = _numDigitsInInteger(vertices.size());
    int facedigits = _numDigitsInInteger(triangles.size());
    snprintf(vertstring, 10, "%u", (unsigned int)vertices.size());
    snprintf(facestring, 10, "%u", (unsigned int)triangles.size());

    int offset = 0;
    int headersize;
    if (isColorEnabled) {
        headersize = 51 + vertdigits + 125 + facedigits + 49;
    } else {
        headersize = 51 + vertdigits + 65 + facedigits + 49;
    }

    int binsize;
    if (isColorEnabled) {
        binsize = headersize + 3*(sizeof(float)*vertices.size() + sizeof(unsigned char)*vertices.size())
                             + (sizeof(unsigned char) + 3*sizeof(int))*triangles.size();
    } else {
        binsize = headersize + 3*sizeof(float)*vertices.size()
                             + (sizeof(unsigned char) + 3*sizeof(int))*triangles.size();
    }
    char *bin = new char[binsize];

    memcpy(bin + offset, header1, 51);
    offset += 51;
    memcpy(bin + offset, vertstring, vertdigits*sizeof(char));
    offset += vertdigits*sizeof(char);

    if (isColorEnabled) { 
        memcpy(bin + offset, header2color, 125);
        offset += 125;
    } else {
        memcpy(bin + offset, header2, 65);
        offset += 65;
    }

    memcpy(bin + offset, facestring, facedigits*sizeof(char));
    offset += facedigits*sizeof(char);
    memcpy(bin + offset, header3, 49);
    offset += 49;

    if (isColorEnabled) {
        float *vertdata = new float[3*vertices.size()];
        vmath::vec3 v;
        for (unsigned int i = 0; i < vertices.size(); i++) {
            v = vertices[i];
            vertdata[3*i] = v.x;
            vertdata[3*i + 1] = v.y;
            vertdata[3*i + 2] = v.z;
        }

        unsigned char *colordata = new unsigned char[3*vertexcolors.size()];
        vmath::vec3 c;
        for (unsigned int i = 0; i < vertexcolors.size(); i++) {
            c = vertexcolors[i];
            colordata[3*i] = (unsigned char)((c.x/1.0)*255.0);
            colordata[3*i + 1] = (unsigned char)((c.y/1.0)*255.0);
            colordata[3*i + 2] = (unsigned char)((c.z/1.0)*255.0);
        }

        int vertoffset = 0;
        int coloroffset = 0;
        int vertsize = 3*sizeof(float);
        int colorsize = 3*sizeof(unsigned char);
        for (unsigned int i = 0; i < vertices.size(); i++) {
            memcpy(bin + offset, vertdata + vertoffset, vertsize);
            offset += vertsize;
            vertoffset += 3;

            memcpy(bin + offset, colordata + coloroffset, colorsize);
            offset += colorsize;
            coloroffset += 3;
        }

        delete[] colordata;
        delete[] vertdata;
    } else {
        float *vertdata = new float[3*vertices.size()];
        vmath::vec3 v;
        for (unsigned int i = 0; i < vertices.size(); i++) {
            v = vertices[i];
            vertdata[3*i] = v.x;
            vertdata[3*i + 1] = v.y;
            vertdata[3*i + 2] = v.z;
        }
        memcpy(bin + offset, vertdata, 3*sizeof(float)*vertices.size());
        offset += 3*sizeof(float)*vertices.size();
        delete[] vertdata;
    }

    Triangle t;
    int verts[3];
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        verts[0] = t.tri[0];
        verts[1] = t.tri[1];
        verts[2] = t.tri[2];

        bin[offset] = 0x03;
        offset += sizeof(unsigned char);

        memcpy(bin + offset, verts, 3*sizeof(int));
        offset += 3*sizeof(int);
    }

    std::ofstream erasefile;
    erasefile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    erasefile.close();

    std::ofstream file(filename.c_str(), std::ios::out | std::ios::binary);
    file.write(bin, binsize);
    file.close();

    delete[] bin;
}

int TriangleMesh::_numDigitsInInteger(int num) {
    if (num == 0) {
        return 1;
    }

    int count = 0;
    while(num != 0) {
        num /= 10;
        count++;
    }

    return count;
}

bool triangleSort(const Triangle &a, const Triangle &b)
{
    if (a.tri[0]==b.tri[0]) {
        if (a.tri[1]==b.tri[1]) {
            return a.tri[2] < b.tri[2];
        } else {
            return a.tri[1] < b.tri[1];
        }
    } else {
        return a.tri[0] < b.tri[0];
    }
}

void TriangleMesh::removeDuplicateTriangles() {
    std::vector<Triangle> uniqueTriangles;

    std::sort(triangles.begin(), triangles.end(), triangleSort);
    Triangle last;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        Triangle t = triangles[i];

        if (!_trianglesEqual(t, last)) {
            uniqueTriangles.push_back(t);
        }
        last = t;
    }

    triangles.clear();
    triangles.insert(triangles.end(), uniqueTriangles.begin(), uniqueTriangles.end());
}

void TriangleMesh::updateVertexNormals() {
    normals.clear();
    _updateVertexTriangles();
    
    std::vector<vmath::vec3> facenormals;
    facenormals.reserve((int)triangles.size());
    Triangle t;
    vmath::vec3 v1, v2;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];

        v1 = vertices[t.tri[1]] - vertices[t.tri[0]];
        v2 = vertices[t.tri[2]] - vertices[t.tri[0]];
        vmath::vec3 norm = vmath::normalize(vmath::cross(v1, v2));

        facenormals.push_back(norm);
    }

    vmath::vec3 n;
    for (unsigned int i = 0; i < _vertexTriangles.size(); i++) {
        n = vmath::vec3();
        for (unsigned int j = 0; j < _vertexTriangles[i].size(); j++) {
            n += facenormals[_vertexTriangles[i][j]];
        }

        n = vmath::normalize(n / (float)_vertexTriangles[i].size());
        normals.push_back(n);
    }
}

void TriangleMesh::getFaceNeighbours(unsigned int tidx, std::vector<int> &n) {
    assert(tidx < triangles.size());
    getFaceNeighbours(triangles[tidx], n);
}

void TriangleMesh::getFaceNeighbours(Triangle t, std::vector<int> &n) {
    assert(vertices.size() == _vertexTriangles.size());

    std::vector<int> vn;
    for (int i = 1; i < 3; i++) {
        vn = _vertexTriangles[t.tri[i]];
        n.insert(n.end(), vn.begin(), vn.end());
    }
}

void TriangleMesh::getVertexNeighbours(unsigned int vidx, std::vector<int> &n) {
    assert(vertices.size() == _vertexTriangles.size());
    assert(vidx < vertices.size());
    std::vector<int> vn = _vertexTriangles[vidx];
    n.insert(n.end(), vn.begin(), vn.end());
}

double TriangleMesh::getTriangleArea(int tidx) {
    assert((unsigned int)tidx < triangles.size());

    if ((unsigned int)tidx < _triangleAreas.size()) {
        return _triangleAreas[tidx];
    }

    Triangle t = triangles[tidx];

    vmath::vec3 AB = vertices[t.tri[1]] - vertices[t.tri[0]];
    vmath::vec3 AC = vertices[t.tri[2]] - vertices[t.tri[0]];

    return 0.5f*vmath::length(vmath::cross(AB, AC));
}

bool TriangleMesh::_trianglesEqual(Triangle &t1, Triangle &t2) {
    return t1.tri[0] == t2.tri[0] &&
           t1.tri[1] == t2.tri[1] &&
           t1.tri[2] == t2.tri[2];
}

bool TriangleMesh::isNeighbours(Triangle t1, Triangle t2) {
    std::vector<int> n;
    getFaceNeighbours(t1, n);
    for (unsigned int i = 0; i < n.size(); i++) {
        if (_trianglesEqual(triangles[n[i]], t2)) {
            return true;
        }
    }

    return false;
}

bool TriangleMesh::_getPLYHeader(std::ifstream *file, std::string *header) {
    file->seekg(0, std::ios_base::beg);

    int maxHeaderSize = 2048;
    char headerBufferChars[2048];
    file->read(headerBufferChars, maxHeaderSize);
    std::string headerBufferString(headerBufferChars, 2048);

    std::string endHeaderString("end_header\n");

    std::size_t match = headerBufferString.find(endHeaderString);
    if (match == std::string::npos) {
        return false;
    }

    *header = headerBufferString.substr(0, match + endHeaderString.size());

    return true;
}

bool TriangleMesh::_getElementNumberInPlyHeader(std::string &header, 
                                                std::string &element, int *n) {
    std::size_t match = header.find(element);
    if (match == std::string::npos) {
        return false;
    }

    int startidx = match + element.size();
    int endidx = 0;
    bool numberFound = false;

    for (unsigned int i = startidx; i < header.size(); i++) {
        if (header[i] == '\n') {
            endidx = i - 1;
            numberFound = true;
            break;
        }
    }

    if (!numberFound) {
        return false;
    }

    std::string numberString = header.substr(startidx, endidx - startidx + 1);
    std::istringstream ss(numberString);
    ss >> *n;

    if (ss.fail()) {
        return false;
    }

    return true;
}

bool TriangleMesh::_getNumVerticesInPLYHeader(std::string &header, int *n) {
    std::string vertexString("element vertex ");
    bool success = _getElementNumberInPlyHeader(header, vertexString, n);

    return success;
}

bool TriangleMesh::_getNumFacesInPLYHeader(std::string &header, int *n) {
    std::string faceString("element face ");
    bool success = _getElementNumberInPlyHeader(header, faceString, n);

    return success;
}

bool TriangleMesh::_isVertexColorsEnabledInPLYHeader(std::string &header) {
    std::string colorString("property uchar red\nproperty uchar green\nproperty uchar blue\n");
    std::size_t match = header.find(colorString);
    return match != std::string::npos;
}

bool TriangleMesh::_loadPLYVertexData(std::ifstream *file, std::string &header) {
    int numVertices;
    bool success = _getNumVerticesInPLYHeader(header, &numVertices);
    if (!success) {
        return false;
    }

    if (numVertices == 0) {
        return true;
    }

    bool isColorEnabled = _isVertexColorsEnabledInPLYHeader(header);

    int vertexSize = 3*sizeof(float);
    if (isColorEnabled) {
        vertexSize = 3*sizeof(float) + 3*sizeof(char);
    }

    int vertexDataSize = numVertices*vertexSize;
    int vertexDataOffset = header.size();

    file->seekg(vertexDataOffset, std::ios_base::beg);
    char *vertexData = new char[vertexDataSize];
    if (!file->read(vertexData, vertexDataSize)) {
        return false;
    }

    vertices.reserve(numVertices);
    if (isColorEnabled) {
        vertexcolors.reserve(numVertices);

        vmath::vec3 p;
        int offset = 0;
        for (int i = 0; i < numVertices; i++) {
            memcpy(&p, vertexData + offset, 3*sizeof(float));
            offset += 3*sizeof(float);

            unsigned char r = vertexData[offset + 0];
            unsigned char g = vertexData[offset + 1];
            unsigned char b = vertexData[offset + 2];
            offset += 3*sizeof(char);

            vertices.push_back(p);
            vertexcolors.push_back(vmath::vec3(r / 255.0, g / 255.0, b / 255.0));
        }
    } else {
        vertices.assign((vmath::vec3*)vertexData, (vmath::vec3*)vertexData + numVertices);
    }
    delete[] vertexData;

    return true;
}

bool TriangleMesh::_loadPLYTriangleData(std::ifstream *file, std::string &header) {
    int numVertices;
    bool success = _getNumVerticesInPLYHeader(header, &numVertices);
    if (!success) {
        return false;
    }

    bool isColorEnabled = _isVertexColorsEnabledInPLYHeader(header);

    int vertexSize = 3*sizeof(float);
    if (isColorEnabled) {
        vertexSize = 3*sizeof(float) + 3*sizeof(char);
    }

    int vertexDataSize = numVertices*vertexSize;
    int vertexDataOffset = header.size();

    int numFaces;
    success = _getNumFacesInPLYHeader(header, &numFaces);
    if (!success) {
        return false;
    }

    if (numFaces == 0) {
        return true;
    }

    int faceSize = sizeof(char) + 3*sizeof(int);
    int faceDataSize = numFaces*faceSize;
    int faceDataOffset = vertexDataOffset + vertexDataSize;

    file->seekg(faceDataOffset, std::ios_base::beg);
    char *faceData = new char[faceDataSize];
    if (!file->read(faceData, faceDataSize)) {
        return false;
    }

    int offset = 0;
    Triangle t;
    triangles.reserve(numFaces);
    for (int i = 0; i < numFaces; i++) {
        unsigned int faceverts = faceData[offset];
        offset += sizeof(char);

        if (faceverts != 0x03) {
            return false;
        }

        memcpy(&(t.tri), faceData + offset, 3*sizeof(int));
        offset += 3*sizeof(int);

        if (t.tri[0] < 0 || t.tri[0] >= numVertices || 
            t.tri[1] < 0 || t.tri[1] >= numVertices || 
            t.tri[2] < 0 || t.tri[2] >= numVertices) {
            return false;
        }
        triangles.push_back(t);
    }

    delete[] faceData;

    return true;
}

void TriangleMesh::_updateVertexTriangles() {
    _vertexTriangles.clear();
    _vertexTriangles.reserve(vertices.size());

    for (unsigned int i = 0; i < vertices.size(); i++) {
        std::vector<int> triangles;
        triangles.reserve(14);  // 14 is the maximum number of adjacent triangles
                                // to a vertex
        _vertexTriangles.push_back(triangles);
    }

    Triangle t;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        _vertexTriangles[t.tri[0]].push_back(i);
        _vertexTriangles[t.tri[1]].push_back(i);
        _vertexTriangles[t.tri[2]].push_back(i);
    }

}

void TriangleMesh::_getTriangleGridCellOverlap(Triangle t, GridIndexVector &cells) {
    GridIndexVector testcells(cells.width, cells.height, cells.depth);
    AABB tbbox = AABB(t, vertices);
    Grid3d::getGridCellOverlap(tbbox, _dx, testcells);

    AABB cbbox = AABB(vmath::vec3(), _dx, _dx, _dx);
    for (unsigned int i = 0; i < testcells.size(); i++) {
        cbbox.position = Grid3d::GridIndexToPosition(testcells[i], _dx);
        if (cbbox.isOverlappingTriangle(t, vertices)) {
            cells.push_back(testcells[i]);
        }
    }
}

void TriangleMesh::_updateTriangleGrid() {
    _destroyTriangleGrid();
    _triGrid = Array3d<std::vector<int>>(_gridi, _gridj, _gridk);

    GridIndexVector cells(_gridi, _gridj, _gridk);
    std::vector<int> *triVector;
    Triangle t;
    GridIndex g;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        cells.clear();
        _getTriangleGridCellOverlap(t, cells);

        for (unsigned int j = 0; j < cells.size(); j++) {
            g = cells[j];
            triVector = _triGrid.getPointer(g);
            triVector->push_back(i);
        }
    }
}

void TriangleMesh::_destroyTriangleGrid() {
    std::vector<int> *tris;
    for (int k = 0; k < _triGrid.depth; k++) {
        for (int j = 0; j < _triGrid.height; j++) {
            for (int i = 0; i < _triGrid.width; i++) {
                tris = _triGrid.getPointer(i, j, k);
                tris->clear();
                tris->shrink_to_fit();
            }
        }
    }
    _triGrid = Array3d<std::vector<int> >();
}

void TriangleMesh::_getSurfaceCells(GridIndexVector &cells) {
    std::vector<int> *tris;
    for (int k = 0; k < _triGrid.depth; k++) {
        for (int j = 0; j < _triGrid.height; j++) {
            for (int i = 0; i < _triGrid.width; i++) {
                tris = _triGrid.getPointer(i, j, k);
                if (tris->size() > 0) {
                    cells.push_back(i, j, k);
                }
            }
        }
    }
}

void TriangleMesh::_floodfill(GridIndex g, Array3d<bool> &cells) {
    assert(Grid3d::isGridIndexInRange(g, _gridi, _gridj, _gridk));
    if (cells(g)) {
        return;
    }

    Array3d<bool> isCellDone = Array3d<bool>(_gridi, _gridj, _gridk, false);
    std::queue<GridIndex> queue;
    queue.push(g);
    isCellDone.set(g, true);

    GridIndex gp;
    GridIndex ns[6];
    while (!queue.empty()) {
        gp = queue.front();
        queue.pop();

        Grid3d::getNeighbourGridIndices6(gp, ns);
        for (int i = 0; i < 6; i++) {
            if (Grid3d::isGridIndexInRange(ns[i], _gridi, _gridj, _gridk) && 
                    !cells(ns[i]) && !isCellDone(ns[i])) {
                isCellDone.set(ns[i], true);
                queue.push(ns[i]);
            }
        }

        cells.set(gp, true);
    }
}

void TriangleMesh::getTrianglePosition(unsigned int index, vmath::vec3 tri[3]) {
    assert(index < triangles.size());

    Triangle t = triangles[index];
    int size = (int)vertices.size();
    assert(t.tri[0] < size && t.tri[1] < size && t.tri[2] < size);

    tri[0] = vertices[t.tri[0]];
    tri[1] = vertices[t.tri[1]];
    tri[2] = vertices[t.tri[2]];
}

vmath::vec3 TriangleMesh::getTriangleNormal(unsigned int index) {
    assert(index < triangles.size());

    Triangle t = triangles[index];
    int size = (int)vertices.size();
    assert(t.tri[0] < size && t.tri[1] < size && t.tri[2] < size);

    return vmath::normalize(normals[t.tri[0]] + normals[t.tri[1]] + normals[t.tri[2]]);
}

vmath::vec3 TriangleMesh::getBarycentricCoordinates(unsigned int index, vmath::vec3 p) {
    Triangle t = triangles[index];
    int size = (int)vertices.size();
    assert(t.tri[0] < size && t.tri[1] < size && t.tri[2] < size);

    vmath::vec3 a = vertices[t.tri[0]];
    vmath::vec3 b = vertices[t.tri[1]];
    vmath::vec3 c = vertices[t.tri[2]];
    vmath::vec3 normal = getTriangleNormal(index);

    float areaABC = vmath::dot(normal, vmath::cross((b - a), (c - a)));
    float areaPBC = vmath::dot(normal, vmath::cross((b - p), (c - p)));
    float areaPCA = vmath::dot(normal, vmath::cross((c - p), (a - p)));

    float bx = areaPBC / areaABC;
    float by = areaPCA / areaABC;
    float bz = 1.0f - bx - by;

    return vmath::vec3(bx, by, bz);
}

vmath::vec3 TriangleMesh::getTriangleNormalSmooth(unsigned int index, vmath::vec3 p) {
    assert(index < triangles.size());

    Triangle t = triangles[index];
    int size = (int)vertices.size();
    assert(t.tri[0] < size && t.tri[1] < size && t.tri[2] < size);

    vmath::vec3 bary = getBarycentricCoordinates(index, p);

    return bary.x*normals[t.tri[0]] + bary.y*normals[t.tri[1]] + bary.z*normals[t.tri[2]];
}

vmath::vec3 TriangleMesh::getTriangleFaceDirection(unsigned int index) {
    assert(index < triangles.size());

    Triangle t = triangles[index];
    int size = (int)vertices.size();
    assert(t.tri[0] < size && t.tri[1] < size && t.tri[2] < size);

    return normals[t.tri[0]] + normals[t.tri[1]] + normals[t.tri[2]];
}

vmath::vec3 TriangleMesh::getTriangleCenter(unsigned int index) {
    assert(index < triangles.size());

    Triangle t = triangles[index];
    int size = (int)vertices.size();
    assert(t.tri[0] < size && t.tri[1] < size && t.tri[2] < size);

    return (vertices[t.tri[0]] + vertices[t.tri[1]] + vertices[t.tri[2]]) / 3.0f;
}

bool TriangleMesh::_isOnTriangleEdge(double u, double v) {
    double eps = 10e-6*_dx;

    if (fabs(u) < eps) {
        return true;
    }

    if (fabs(v) < eps || fabs(u + v - 1.0) < eps) {
        return true;
    }

    return false;
}

bool TriangleMesh::_isTriangleInVector(int index, std::vector<int> &tris) {
    for (unsigned int i = 0; i < tris.size(); i++) {
        if (_trianglesEqual(triangles[index], triangles[tris[i]])) {
            return true;
        }
    }
    return false;
}

int TriangleMesh::_getIntersectingTrianglesInCell(GridIndex g, vmath::vec3 p, vmath::vec3 dir,
                                                  std::vector<int> &tris, bool *success) {
    if (_triGrid(g).size() == 0) {
        *success = true;
        return 0;
    }

    // There are cases where this method could return an incorrect number of
    // surface intersections. If a line intersects at exactly an edge or vertex,
    // the number of intersections could be counted incorrectly as 2 or 3.
    // If it is detected that a line has intersected with an edge or vertex,
    // mark *success as false and return 0
    std::vector<int> *indices = _triGrid.getPointer(g);
    vmath::vec3 collision;
    vmath::vec3 tri[3];
    double u, v;
    int numIntersections = 0;

    numIntersections = 0;
    for (unsigned int i = 0; i < indices->size(); i++) {
        getTrianglePosition(indices->at(i), tri);

        bool isIntersecting = Collision::lineIntersectsTriangle(p, dir, 
                                                                tri[0], tri[1], tri[2],
                                                                &collision, &u, &v);
        if (!isIntersecting) { continue; }

        if (_isOnTriangleEdge(u, v)) {
            *success = false;
            return 0;
        }

        if (!_isTriangleInVector(indices->at(i), tris)) {
            tris.push_back(indices->at(i));
            numIntersections++;
        }
    }

    *success = true;
    return numIntersections;
}

bool TriangleMesh::_isIntInVector(int v, std::vector<int> &ints) {
    for (unsigned int i = 0; i < ints.size(); i++) {
        if (ints[i] == v) {
            return true;
        }
    }
    return false;
}

bool TriangleMesh::_isCellInsideMesh(const GridIndex g) {
    // count how many intersections between point and edge of grid
    // even intersections: outside
    // odd intersections: inside
    assert(Grid3d::isGridIndexInRange(g, _gridi, _gridj, _gridk));
    assert(_triGrid(g).size() == 0);

    // Add a random jitter to the center position of the cell.
    // If the line position is exactly in the center, intersections
    // will be more likely to occur on triangle edges and the method
    // _getIntersectingTrianglesInCell method will choose to safely fail.
    // The likeliness of edge intersections is due to symmetries in the 
    // polygonization method. 
    double jit = 0.1*_dx;
    vmath::vec3 jitter = vmath::vec3(_randomFloat(-jit, jit),
                                 _randomFloat(-jit, jit),
                                 _randomFloat(-jit, jit));

    vmath::vec3 p = Grid3d::GridIndexToPosition(g, _dx) + 0.5f*vmath::vec3(_dx, _dx, _dx) + jitter;
    vmath::vec3 dir = vmath::vec3(1.0, 0.0, 0.0);
    

    std::vector<int> allIntersections;
    std::vector<int> leftIntersections;
    std::vector<int> rightIntersections;
    std::vector<int> intersections;
    GridIndex n = GridIndex(g.i - 1, g.j, g.k);
    while (Grid3d::isGridIndexInRange(n, _gridi, _gridj, _gridk)) {
        intersections.clear();
        bool success;
        _getIntersectingTrianglesInCell(n, p, dir, intersections, &success);
        if (!success) {
            return false;
        }

        for (unsigned int i = 0; i < intersections.size(); i++) {
            int idx = intersections[i];
            if (!_isIntInVector(idx, allIntersections) && !_isTriangleInVector(idx, allIntersections)) {
                leftIntersections.push_back(idx);
                allIntersections.push_back(idx);
            }
        }
        n = GridIndex(n.i - 1, n.j, n.k);
    }

    n = GridIndex(g.i + 1, g.j, g.k);
    while (Grid3d::isGridIndexInRange(n, _gridi, _gridj, _gridk)) {
        intersections.clear();
        bool success;
        _getIntersectingTrianglesInCell(n, p, dir, intersections, &success);
        
        if (!success) {
            return false;
        }

        for (unsigned int i = 0; i < intersections.size(); i++) {
            int idx = intersections[i];
            if (!_isIntInVector(idx, allIntersections) && !_isTriangleInVector(idx, allIntersections)) {
                rightIntersections.push_back(idx);
                allIntersections.push_back(idx);
            }
        }
        n = GridIndex(n.i + 1, n.j, n.k);
    }

    assert(leftIntersections.size() % 2 == rightIntersections.size() % 2);

    return leftIntersections.size() % 2 == 1;
}

void TriangleMesh::getCellsInsideMesh(GridIndexVector &cells) {
    if (_gridi == 0 || _gridj == 0 || _gridk == 0) {
        return;
    }

    assert(cells.width == _gridi && cells.height == _gridj && cells.depth == _gridk);

    // find all cells that are on the surface boundary.
    // Iterate through surface cells and test if any of their
    // 6 neighbours are inside the mesh. If a cell is inside the mesh,
    // floodfill that region.

    _updateTriangleGrid();

    GridIndexVector surfaceCells(_gridi, _gridj, _gridk);
    _getSurfaceCells(surfaceCells);

    Array3d<bool> insideCellGrid = Array3d<bool>(_gridi, _gridj, _gridk, false);
    for (unsigned int i = 0; i < surfaceCells.size(); i++) {
        insideCellGrid.set(surfaceCells[i], true);
    }

    GridIndex neighbours[6];
    GridIndex n;
    for (unsigned int i = 0; i < surfaceCells.size(); i++) {
        Grid3d::getNeighbourGridIndices6(surfaceCells[i], neighbours);
        for (int j = 0; j < 6; j++) {
            n = neighbours[j];
            if (Grid3d::isGridIndexInRange(n, _gridi, _gridj, _gridk) && 
                    !insideCellGrid(n) && _isCellInsideMesh(n)) {
                _floodfill(n, insideCellGrid);
                break;
            }
        }
    }
    
    for (int k = 0; k < _triGrid.depth; k++) {
        for (int j = 0; j < _triGrid.height; j++) {
            for (int i = 0; i < _triGrid.width; i++) {
                if (insideCellGrid(i, j, k)) {
                    cells.push_back(i, j, k);
                }
            }
        }
    }
    
    _destroyTriangleGrid();
}

void TriangleMesh::_smoothTriangleMesh(double value, std::vector<bool> &isSmooth) {
    std::vector<vmath::vec3> newvertices;
    newvertices.reserve(vertices.size());

    vmath::vec3 v;
    vmath::vec3 nv;
    vmath::vec3 avg;
    Triangle t;
    for (unsigned int i = 0; i < vertices.size(); i++) {

        if (!isSmooth[i]) {
            newvertices.push_back(vertices[i]);
            continue;
        }

        int count = 0;
        avg = vmath::vec3();
        for (unsigned int j = 0; j < _vertexTriangles[i].size(); j++) {
            t = triangles[_vertexTriangles[i][j]];
            if (t.tri[0] != (int)i) {
                avg += vertices[t.tri[0]];
                count++;
            }
            if (t.tri[1] != (int)i) {
                avg += vertices[t.tri[1]];
                count++;
            }
            if (t.tri[2] != (int)i) {
                avg += vertices[t.tri[2]];
                count++;
            }
        }

        avg /= (float)count;
        v = vertices[i];
        nv = v + (float)value * (avg - v);
        newvertices.push_back(nv);
    }

    vertices = newvertices;
}

void TriangleMesh::_getBoolVectorOfSmoothedVertices(std::vector<int> &verts, 
                                                    std::vector<bool> &isVertexSmooth) {
    isVertexSmooth.assign(vertices.size(), false);
    for (unsigned int i = 0; i < verts.size(); i++) {
        assert(verts[i] >= 0 && (unsigned int)verts[i] < vertices.size());
        isVertexSmooth[verts[i]] = true;
    }
}

void TriangleMesh::smooth(double value, int iterations) {
    std::vector<int> verts;
    verts.reserve(vertices.size());
    for (unsigned int i = 0; i < vertices.size(); i++) {
        verts.push_back(i);
    }

    smooth(value, iterations, verts);
}

void TriangleMesh::smooth(double value, int iterations, 
                          std::vector<int> &verts) {
    value = value < 0.0 ? 0.0 : value;
    value = value > 1.0 ? 1.0 : value;

    std::vector<bool> isVertexSmooth;
    _getBoolVectorOfSmoothedVertices(verts, isVertexSmooth);

    _vertexTriangles.clear();
    _updateVertexTriangles();
    for (int i = 0; i < iterations; i++) {
        _smoothTriangleMesh(value, isVertexSmooth);
    }
    _vertexTriangles.clear();

    updateVertexNormals();
}

void TriangleMesh::updateVertexTriangles() {
    _updateVertexTriangles();
}

void TriangleMesh::clearVertexTriangles() {
    _vertexTriangles.clear();
}

void TriangleMesh::updateTriangleAreas() {
    _triangleAreas.clear();
    for (unsigned int i = 0; i < triangles.size(); i++) {
        _triangleAreas.push_back(getTriangleArea(i));
    }
}

void TriangleMesh::clearTriangleAreas() {
    _triangleAreas.clear();
}

void TriangleMesh::_getPolyhedronFromTriangle(int tidx, 
                                              std::vector<bool> &visitedTriangles,
                                              std::vector<int> &polyhedron) {

    assert(!visitedTriangles[tidx]);

    std::vector<int> queue;
    queue.push_back(tidx);
    visitedTriangles[tidx] = true;

    std::vector<int> neighbours;
    while (!queue.empty()) {
        int t = queue.back();
        queue.pop_back();

        neighbours.clear();
        getFaceNeighbours(t, neighbours);
        for (unsigned int i = 0; i < neighbours.size(); i++) {
            int n = neighbours[i];

            if (!visitedTriangles[n]) {
                queue.push_back(n);
                visitedTriangles[n] = true;
            }
        }

        polyhedron.push_back(t);
    }
}

void TriangleMesh:: _getPolyhedra(std::vector<std::vector<int> > &polyList) {
    updateVertexTriangles();

    std::vector<bool> visitedTriangles = std::vector<bool>(triangles.size(), false);
    for (unsigned int i = 0; i < visitedTriangles.size(); i++) {
        if (!visitedTriangles[i]) {
            std::vector<int> polyhedron;
            _getPolyhedronFromTriangle(i, visitedTriangles, polyhedron);
            polyList.push_back(polyhedron);
        }
    }

    clearVertexTriangles();
}

double TriangleMesh::_getSignedTriangleVolume(unsigned int tidx) {
    vmath::vec3 p1 = vertices[triangles[tidx].tri[0]];
    vmath::vec3 p2 = vertices[triangles[tidx].tri[1]];
    vmath::vec3 p3 = vertices[triangles[tidx].tri[2]];

    double v321 = p3.x*p2.y*p1.z;
    double v231 = p2.x*p3.y*p1.z;
    double v312 = p3.x*p1.y*p2.z;
    double v132 = p1.x*p3.y*p2.z;
    double v213 = p2.x*p1.y*p3.z;
    double v123 = p1.x*p2.y*p3.z;

    return (1.0/6.0)*(-v321 + v231 + v312 - v132 - v213 + v123);
}

double TriangleMesh::_getPolyhedronVolume(std::vector<int> &polyhedron) {
    double sum = 0.0;
    for (unsigned int i = 0; i < polyhedron.size(); i++) {
        sum += _getSignedTriangleVolume(i);
    }

    return fabs(sum);
}

void TriangleMesh::removeExtraneousVertices() {

    std::vector<bool> unusedVertices = std::vector<bool>(vertices.size(), true);
    Triangle t;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        unusedVertices[t.tri[0]] = false;
        unusedVertices[t.tri[1]] = false;
        unusedVertices[t.tri[2]] = false;
    }

    int unusedCount = 0;
    for (unsigned int i = 0; i < unusedVertices.size(); i++) {
        if (unusedVertices[i]) {
            unusedCount++;
        }
    }

    if (unusedCount == 0) {
        return;
    }

    bool hasVertexColors = vertices.size() == vertexcolors.size();

    std::vector<int> indexTranslationTable = std::vector<int>(vertices.size(), -1);
    std::vector<vmath::vec3> newVertexList;
    std::vector<vmath::vec3> newVertexColorList;

    newVertexList.reserve(vertices.size() - unusedCount);
    if (hasVertexColors) {
        newVertexColorList.reserve(vertices.size() - unusedCount);
    }

    int vidx = 0;
    for (unsigned int i = 0; i < unusedVertices.size(); i++) {
        if (!unusedVertices[i]) {
            newVertexList.push_back(vertices[i]);
            indexTranslationTable[i] = vidx;
            vidx++;

            if (hasVertexColors) {
                newVertexColorList.push_back(vertexcolors[i]);
            }
        }
    }
    newVertexList.shrink_to_fit();
    newVertexColorList.shrink_to_fit();

    vertices = newVertexList;
    if (hasVertexColors) {
        vertexcolors = newVertexColorList;
    }

    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        t.tri[0] = indexTranslationTable[t.tri[0]];
        t.tri[1] = indexTranslationTable[t.tri[1]];
        t.tri[2] = indexTranslationTable[t.tri[2]];
        assert(t.tri[0] != -1 && t.tri[1] != -1 && t.tri[2] != -1);

        triangles[i] = t;
    }

    updateVertexNormals();
}

void TriangleMesh::removeTriangles(std::vector<int> &removalTriangles) {
    std::vector<bool> invalidTriangles = std::vector<bool>(triangles.size(), false);
    for (unsigned int i = 0; i < removalTriangles.size(); i++) {
        int tidx = removalTriangles[i];
        invalidTriangles[tidx] = true;
    }

    std::vector<Triangle> newTriangleList;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        if (!invalidTriangles[i]) {
            newTriangleList.push_back(triangles[i]);
        }
    }

    triangles = newTriangleList;
}

void TriangleMesh::removeMinimumVolumePolyhedra(double volume) {
    if (volume <= 0.0) {
        return;
    }

    std::vector<std::vector<int> > polyList;
    _getPolyhedra(polyList);

    std::vector<int> removalTriangles;
    for (unsigned int i = 0; i < polyList.size(); i++) {
        if (_getPolyhedronVolume(polyList[i]) <= volume) {
            for (unsigned int j = 0; j < polyList[i].size(); j++) {
                removalTriangles.push_back(polyList[i][j]);
            }
        }
    }

    if (removalTriangles.size() == 0) {
        return;
    }

    removeTriangles(removalTriangles);
    removeExtraneousVertices();
}

void TriangleMesh::removeMinimumTriangleCountPolyhedra(int count) {
    if (count <= 0) {
        return;
    }

    std::vector<std::vector<int> > polyList;
    _getPolyhedra(polyList);

    std::vector<int> removalTriangles;
    for (unsigned int i = 0; i < polyList.size(); i++) {
        if ((int)polyList[i].size() <= count) {
            for (unsigned int j = 0; j < polyList[i].size(); j++) {
                removalTriangles.push_back(polyList[i][j]);
            }
        }
    }

    if (removalTriangles.size() == 0) {
        return;
    }

    removeTriangles(removalTriangles);
    removeExtraneousVertices();
}

bool TriangleMesh::_isPolyhedronHole(std::vector<int> &poly) {

    if (poly.size() == 0) {
        return false;
    }

    // TEST - DELETE
    if (poly.size() > 10000) {
        return false;
    }
    // END TEST

    vmath::vec3 centroid;
    for (unsigned int i = 0; i < poly.size(); i++) {
        centroid += getTriangleCenter(poly[i]);
    }
    centroid /= poly.size();

    Triangle t;
    vmath::vec3 p, vn, normal;
    double sum = 0;
    for (unsigned int i = 0; i < poly.size(); i++) {
        t = triangles[poly[i]];
        normal = Collision::getTriangleNormal(vertices[t.tri[0]], 
                                              vertices[t.tri[1]], 
                                              vertices[t.tri[2]]);
        p = getTriangleCenter(poly[i]);
        vn = p - centroid;
        double dot = vmath::dot(vn, normal);
        sum += dot;
    }

    return sum < 0;
}

void TriangleMesh::removeHoles() {
    std::vector<std::vector<int> > polyList;
    _getPolyhedra(polyList);

    std::vector<int> removalTriangles;
    for (unsigned int i = 0; i < polyList.size(); i++) {
        if (_isPolyhedronHole(polyList[i])) {
            for (unsigned int j = 0; j < polyList[i].size(); j++) {
                removalTriangles.push_back(polyList[i][j]);
            }
        }
    }

    if (removalTriangles.size() == 0) {
        return;
    }

    removeTriangles(removalTriangles);
    removeExtraneousVertices();
}

void TriangleMesh::translate(vmath::vec3 trans) {
    for (unsigned int i = 0; i < vertices.size(); i++) {
        vertices[i] += trans;
    }
}

void TriangleMesh::append(TriangleMesh &mesh) {
    vertices.reserve(vertices.size() + mesh.vertices.size());
    vertexcolors.reserve(vertexcolors.size() + mesh.vertexcolors.size());
    normals.reserve(normals.size() + mesh.normals.size());
    triangles.reserve(triangles.size() + mesh.triangles.size());

    int indexOffset = vertices.size();

    vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
    vertexcolors.insert(vertexcolors.end(), mesh.vertexcolors.begin(), mesh.vertexcolors.end());
    normals.insert(normals.end(), mesh.normals.begin(), mesh.normals.end());

    Triangle t;
    for (unsigned int i = 0; i < mesh.triangles.size(); i++) {
        t = mesh.triangles[i];
        t.tri[0] += indexOffset;
        t.tri[1] += indexOffset;
        t.tri[2] += indexOffset;

        triangles.push_back(t);
    }
}

void TriangleMesh::join(TriangleMesh &mesh) {
    double tol = 10e-5;
    join(mesh, tol);
}

void TriangleMesh::join(TriangleMesh &mesh, double tolerance) {
    if (mesh.vertices.size() == 0) {
        return;
    }

    if (vertices.size() == 0) {
        append(mesh);
        return;
    }

    AABB bbox = _getMeshVertexIntersectionAABB(vertices, mesh.vertices, tolerance);

    unsigned int indexOffset = vertices.size();
    append(mesh);

    std::vector<int> verts1;
    for (unsigned int i = 0; i < indexOffset; i++) {
        if (bbox.isPointInside(vertices[i])) {
            verts1.push_back(i);
        }
    }

    std::vector<int> verts2;
    for (unsigned int i = indexOffset; i < vertices.size(); i++) {
        if (bbox.isPointInside(vertices[i])) {
            verts2.push_back(i);
        }
    }

    std::vector<std::pair<int, int> > vertexPairs;
    _findDuplicateVertexPairs(verts1, verts2, bbox, tolerance, vertexPairs);

    std::vector<int> indexTable;
    indexTable.reserve(vertices.size());
    for (unsigned int i = 0; i < vertices.size(); i++) {
        indexTable.push_back(i);
    }

    for (unsigned int i = 0; i < vertexPairs.size(); i++) {
        indexTable[vertexPairs[i].second] = vertexPairs[i].first;
    }

    Triangle t;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        t.tri[0] = indexTable[t.tri[0]];
        t.tri[1] = indexTable[t.tri[1]];
        t.tri[2] = indexTable[t.tri[2]];

        if (t.tri[0] == t.tri[1] || t.tri[1] == t.tri[2] || t.tri[2] == t.tri[0]) {
            // Don't collapse triangles
            continue;
        }

        triangles[i] = t;
    }

    removeExtraneousVertices();
}

AABB TriangleMesh::_getMeshVertexIntersectionAABB(std::vector<vmath::vec3> verts1,
                                                  std::vector<vmath::vec3> verts2, 
                                                  double tolerance) {
    AABB bbox1(verts1);
    AABB bbox2(verts2);

    bbox1.expand(2.0*tolerance);
    bbox2.expand(2.0*tolerance);

    AABB inter = bbox1.getIntersection(bbox2);

    return inter;
}

bool sortVertexPairByFirstIndex(const std::pair<int, int> &a,
                                const std::pair<int, int> &b) { 
    return a.first < b.first;
}

// Unique list of vertex pair indices sorted in order of first index.
// For each pair, first < second
void TriangleMesh::_findDuplicateVertexPairs(int i, int j, int k, double dx, 
                                             std::vector<std::pair<int, int> > &vertexPairs) {
    SpatialPointGrid grid(i, j, k, dx);
    std::vector<GridPointReference> refs = grid.insert(vertices);

    std::vector<bool> isPaired(vertices.size(), false);

    double eps = 10e-6;
    std::vector<GridPointReference> query;
    for (unsigned int i = 0; i < vertices.size(); i++) {

        if (isPaired[i]) {
            continue;
        }

        query.clear();
        grid.queryPointReferencesInsideSphere(refs[i], eps, query);

        if (query.size() == 0) {
            continue;
        }

        GridPointReference closestRef;
        if (query.size() == 1) {
            closestRef = query[0];
        } else {
            double mindist = std::numeric_limits<double>::infinity();

            for (unsigned int idx = 0; idx < query.size(); idx++) {
                vmath::vec3 v = vertices[i] - vertices[query[idx].id];
                double distsq = vmath::lengthsq(v);
                if (distsq < mindist) {
                    mindist = distsq;
                    closestRef = query[idx];
                }
            }
        }

        int pair1 = i;
        int pair2 = closestRef.id;
        if (pair2 < pair1) {
            pair1 = closestRef.id;
            pair2 = i;
        }

        vertexPairs.push_back(std::pair<int, int>(pair1, pair2));
        isPaired[closestRef.id] = true;
    }

    std::sort(vertexPairs.begin(), vertexPairs.end(), sortVertexPairByFirstIndex);
}

// matches vertex pairs between verts1 and verts2
// AABB bbox bounds verts1 and verts2
void TriangleMesh::_findDuplicateVertexPairs(std::vector<int> &verts1, 
                                             std::vector<int> &verts2, 
                                             AABB bbox,
                                             double tolerance, 
                                             std::vector<std::pair<int, int> > &vertexPairs) {

    double dx = 0.0625;
    int isize = (int)ceil(bbox.width / dx);
    int jsize = (int)ceil(bbox.height / dx);
    int ksize = (int)ceil(bbox.depth / dx);

    vmath::vec3 offset = bbox.position;
    std::vector<vmath::vec3> gridpoints;
    gridpoints.reserve(verts2.size());
    for (unsigned int i = 0; i < verts2.size(); i++) {
        gridpoints.push_back(vertices[verts2[i]] - offset);
    }

    SpatialPointGrid grid(isize, jsize, ksize, dx);
    grid.insert(gridpoints);

    double eps = tolerance;
    std::vector<GridPointReference> query;
    for (unsigned int i = 0; i < verts1.size(); i++) {

        vmath::vec3 v1 = vertices[verts1[i]] - offset;
        query.clear();
        grid.queryPointReferencesInsideSphere(v1, eps, query);

        if (query.size() == 0) {
            continue;
        }

        GridPointReference closestRef;
        if (query.size() == 1) {
            closestRef = query[0];
        } else {
            double mindist = std::numeric_limits<double>::infinity();

            for (unsigned int idx = 0; idx < query.size(); idx++) {
                vmath::vec3 v = vertices[i] - vertices[query[idx].id];
                double distsq = vmath::lengthsq(v);
                if (distsq < mindist) {
                    mindist = distsq;
                    closestRef = query[idx];
                }
            }
        }

        int pair1 = verts1[i];
        int pair2 = verts2[closestRef.id];

        vertexPairs.push_back(std::pair<int, int>(pair1, pair2));
    }
}

void TriangleMesh::removeDuplicateVertices(int i, int j, int k, double dx) {

    std::vector<std::pair<int, int> > vertexPairs;
    _findDuplicateVertexPairs(i, j, k, dx, vertexPairs);

    std::vector<int> indexTable;
    indexTable.reserve(vertices.size());
    for (unsigned int i = 0; i < vertices.size(); i++) {
        indexTable.push_back(i);
    }

    for (unsigned int i = 1; i < vertexPairs.size(); i++) {
        indexTable[vertexPairs[i].second] = vertexPairs[i].first;
    }

    Triangle t;
    for (unsigned int i = 0; i < triangles.size(); i++) {
        t = triangles[i];
        t.tri[0] = indexTable[t.tri[0]];
        t.tri[1] = indexTable[t.tri[1]];
        t.tri[2] = indexTable[t.tri[2]];

        if (t.tri[0] == t.tri[1] || t.tri[1] == t.tri[2] || t.tri[2] == t.tri[0]) {
            // Don't collapse triangles
            continue;
        }

        triangles[i] = t;
    }

    removeExtraneousVertices();
}