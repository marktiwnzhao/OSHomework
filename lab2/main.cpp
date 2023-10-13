#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>

using namespace std;

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;

#pragma pack(1)             //设置1字节对齐
using BPB = struct BPB {
    u16 BPB_BytesPerSector; // 每个扇区的字节数
    u8 BPB_SecPerCluster;   // 每个簇的扇区数
    u16 BPB_RsvSecCnt;      // Boot record占用的扇区数
    u8 BPB_NumFATs;         // FAT的数量，一般为 2
    u16 BPB_RootEntCnt;     // 根目录文件数的最大值
    u16 BPB_TotSec16;       // 扇区总数
    u8 BPB_Media;           // 媒体描述符
    u16 BPB_FATSz16;        // 一个FAT的扇区数
    u16 BPB_SecPerTrk;      // 每个磁道扇区数
    u16 BPB_NumHeads;       // 磁头数
    u32 BPB_HiddenSec;      // 隐藏扇区数
    u32 BPB_TotSec32;       // 如果BPB_TotSec16是 0，由这个值记录扇区总数
};

using Entry = struct Entry {
    char DIR_Name[11];      // 文件名 8 字节，扩展名 3 字节
    u8 DIR_Attr;            // 文件属性
    u8 reserved[10];        // 保留位
    u16 DIR_WrtTime;        // 最后一次写入时间
    u16 DIR_WrtDate;        // 最后一次写入日期
    u16 DIR_FstClus;        // 文件的开始簇号
    u32 DIR_FileSize;       // 文件大小
};
#pragma pack()              // 取消指定对齐，恢复默认对齐

// 文件节点
using FNode = struct FNode {
    string path;    // 路径名
    string name;    // 文件名
    int cluster;    // 文件所在的开始簇号
    u32 fileSize;   // 文件大小
};
// 目录节点
using DNode = struct DNode {
    DNode* father;              // 父目录
    string path;                // 路径名
    string name;                // 目录名
    vector<DNode*> DiSubDir;    // 直接子目录
    vector<FNode*> DiSubFile;   // 直接子文件
};

// 全局变量
BPB bpb;
vector<Entry> rootEntries;
char* fat;
ifstream in;
DNode* root = new DNode;

extern "C" {
    void sprint(const char*, int color);
}

void printRed(const string& s) {
    sprint(s.c_str(), 1);
}
void printWhite(const string& s) {
    sprint(s.c_str(), 0);
}
// 分割字符串
vector<string> split(const string& str, const string& delim) {
    vector<string> res;
    if(str.empty()) return res;
    //先将要切割的字符串从string类型转换为char*类型
    char* strs = new char[str.length() + 1]; //不要忘了
    strcpy(strs, str.c_str());

    char* d = new char[delim.length() + 1];
    strcpy(d, delim.c_str());

    char* p = strtok(strs, d);
    while(p) {
        string s = p;        //分割得到的字符串转换为string类型
        res.push_back(s);    //存入结果数组
        p = strtok(NULL, d);
    }
    delete[] strs;
    delete[] d;
    return res;
}
// 得到扩展名
string getExtension(const Entry& entry) {
    string res;
    for(int i = 8; i < 11; i++) {
        if(entry.DIR_Name[i] != ' ') {
            res += entry.DIR_Name[i];
        }
    }
    return res;
}
// 得到该目录项对应的名字
string getName(const Entry& entry) {
    string res;
    string extension = getExtension(entry);
    for(int i = 0; i < 8; i++) {
        if(entry.DIR_Name[i] != ' ') {
            res += entry.DIR_Name[i];
        } else break;
    }
    if(!extension.empty()) {
        res += ".";
        res += extension;
    }
    return res;
}
// 根据当前簇号得到下一个簇号
int nextCluster(int cluster) {
    int startByte = (cluster / 2) * 3;
    u8 firstByte = fat[startByte];
    u8 secondByte = fat[startByte+1];
    u8 thirdByte = fat[startByte+2];
    if(cluster % 2 == 0) {   //当前为偶数号数据簇，第一个字节作为低八位，第二个字节前4位作为高4位
        cluster = firstByte | ((secondByte & 0x0f) << 8);
    } else {
        cluster = (thirdByte << 4) | ((secondByte >> 4) & 0x0f);
    }
    return cluster;
}
// 加载磁盘镜像的目录和文件
void Load(DNode& dirNode, int cluster) {
    FNode* fNode;
    DNode* dNode;
    // 如果是根目录
    if(dirNode.name == "/") {
        for(int i = 0; i < rootEntries.size(); i++) {
            if(rootEntries[i].DIR_Name[0] != '\xe5') {
                if(rootEntries[i].DIR_Attr == 0x20) {
                    fNode = new FNode;
                    fNode->name = getName(rootEntries[i]);
                    fNode->path = dirNode.path + fNode->name;
                    fNode->cluster = rootEntries[i].DIR_FstClus;
                    fNode->fileSize = rootEntries[i].DIR_FileSize;
                    dirNode.DiSubFile.push_back(fNode);         // 将文件节点添加到直接子文件中
                } else if(rootEntries[i].DIR_Attr == 0x10) {
                    dNode = new DNode;
                    dNode->father = &dirNode;
                    dNode->name = getName(rootEntries[i]);
                    dNode->path = dirNode.path + dNode->name + "/";
                    dirNode.DiSubDir.push_back(dNode);          // 将目录节点添加到直接子目录中
                    Load(*dNode, rootEntries[i].DIR_FstClus);
                }
            }
        }
        return;
    }
    // 如果是一个非根目录
    Entry tmp;
    string name;
    int p;
    // 读相应的扇区，遍历所有的目录项
    while(cluster < 0xff8) {
        p = (bpb.BPB_RsvSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz16 + cluster - 2) * bpb.BPB_BytesPerSector + bpb.BPB_RootEntCnt * 32;
        for(int i = 0; i < bpb.BPB_BytesPerSector / 32; i++) {
            in.seekg(p, ios::beg);
            p += sizeof(tmp);
            in.read((char*) &tmp, sizeof(tmp));
            if(tmp.DIR_Name[0] != '\xe5') {
                name = getName(tmp);
                if(name != "." && name != "..") {
                    if(tmp.DIR_Attr == 0x20) {
                        fNode = new FNode;
                        fNode->name = getName(tmp);
                        fNode->path = dirNode.path + fNode->name;
                        fNode->cluster = tmp.DIR_FstClus;
                        fNode->fileSize = tmp.DIR_FileSize;
                        dirNode.DiSubFile.push_back(fNode);
                    } else if(tmp.DIR_Attr == 0x10) {
                        dNode = new DNode;
                        dNode->father = &dirNode;
                        dNode->name = getName(tmp);
                        dNode->path = dirNode.path + dNode->name + "/";
                        dirNode.DiSubDir.push_back(dNode);
                        Load(*dNode, tmp.DIR_FstClus);
                    }
                }
            }
        }
        cluster = nextCluster(cluster);
    }
}
// cat输出
void catOut(int cluster, u32 fileSize) {
    char* content = new char[fileSize];
    char* p = content;
    u32 restSize = fileSize;
    while(true) {
        in.seekg((bpb.BPB_RsvSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz16 + cluster - 2) * bpb.BPB_BytesPerSector + bpb.BPB_RootEntCnt * 32, ios::beg);
        if(restSize >= bpb.BPB_BytesPerSector) {
            in.read(p, bpb.BPB_BytesPerSector);
            p += bpb.BPB_BytesPerSector;
            restSize -= bpb.BPB_BytesPerSector;
        } else {
            in.read(p, restSize);
            break;
        }
        cluster = nextCluster(cluster);
    }
    string s;
    for(int i = 0; i < fileSize; i++) {
        s += content[i];
    }
    // cout << s;
    printWhite(s);
    delete[] content;
}
// ls输出
void lsOut(DNode* dNode) {
    //cout << dNode->path << ":" << endl;
    printWhite(dNode->path + ":" + "\n");
    if(dNode->path != "/") {
        //cout << "." << " ";
        printRed(".  ");
        if(dNode->DiSubDir.empty() && dNode->DiSubFile.empty()) {
            //cout << ".." << endl;
            printRed("..\n");
        } else {
            //cout << ".." << " ";
            printRed("..  ");
        }
    }
    for(int i = 0; i< dNode->DiSubDir.size(); i++) {
        if(i == dNode->DiSubDir.size()-1 && dNode->DiSubFile.empty()) {
            //cout << dNode->DiSubDir[i]->name << endl;
            printRed(dNode->DiSubDir[i]->name + "\n");
        } else {
            //cout << dNode->DiSubDir[i]->name << " ";
            printRed(dNode->DiSubDir[i]->name + "  ");
        }
    }
    for(int i = 0; i < dNode->DiSubFile.size(); i++) {
        if(i == dNode->DiSubFile.size()-1) {
            //cout << dNode->DiSubFile[i]->name << endl;
            printWhite(dNode->DiSubFile[i]->name + "\n");
        } else {
            //cout << dNode->DiSubFile[i]->name << " ";
            printWhite(dNode->DiSubFile[i]->name + "  ");
        }
    }
    for(int i = 0; i < dNode->DiSubDir.size(); i++) {
        lsOut(dNode->DiSubDir[i]);
    }
}
// ls -l输出
void lslOut(DNode* dNode) {
    //cout << dNode->path << " ";
    //cout << dNode->DiSubDir.size() << " ";
    //cout << dNode->DiSubFile.size() << ":" << endl;
    printWhite(dNode->path + " ");
    printWhite(to_string(dNode->DiSubDir.size()) + " ");
    printWhite(to_string(dNode->DiSubFile.size()) + ":" + "\n");
    if(dNode->path != "/") {
        //cout << "." << endl;
        printRed(".\n");
        //cout << ".." << endl;
        printRed("..\n");
    }
    for(int i = 0; i < dNode->DiSubDir.size(); i++) {
        //cout << dNode->DiSubDir[i]->name << " ";
        //cout << dNode->DiSubDir[i]->DiSubDir.size() << " ";
        //cout << dNode->DiSubDir[i]->DiSubFile.size() << endl;
        printRed(dNode->DiSubDir[i]->name + " ");
        printWhite(to_string(dNode->DiSubDir[i]->DiSubDir.size()) + " ");
        printWhite(to_string(dNode->DiSubDir[i]->DiSubFile.size()) + "\n");

    }
    for(int i = 0; i < dNode->DiSubFile.size(); i++) {
        //cout << dNode->DiSubFile[i]->name << " ";
        //cout << dNode->DiSubFile[i]->fileSize << endl;
        printWhite(dNode->DiSubFile[i]->name + " ");
        printWhite(to_string(dNode->DiSubFile[i]->fileSize) + "\n");
    }
    //cout << endl;
    printWhite("\n");
    for(int i = 0; i < dNode->DiSubDir.size(); i++) {
        lslOut(dNode->DiSubDir[i]);
    }
}
// 解析路径
void parsePath(const string& path, int type) {
    vector<string> p = split(path, "/");
    FNode* f = nullptr;
    DNode* d = root;
    int point = 0;
    bool isFind = false;
    // 如果是根目录
    if(p.empty()) {
        if(type == 0) {
            lsOut(d);
        } else if(type == 1) {
            lslOut(d);
        } else {
            //cout << "Wrong Path!" << endl;
            printRed("Wrong Path!\n");
        }
        return;
    }
    // 如果不是，则逐个查询
    while(point < p.size()) {
        isFind = false;
        for(int i = 0; i < d->DiSubFile.size(); i++) {
            if(d->DiSubFile[i]->name == p[point]) {
                f = d->DiSubFile[i];
                point++;
                isFind = true;
                break;
            }
        }
        if(!isFind) {   // 路径是/./../.也可以
            if(p[point] == ".") {
                point++;
                isFind = true;
            }
            else if(p[point] == "..") {
                point++;
                d = d->father;
                isFind = true;
            } else {
                for(int i = 0; i < d->DiSubDir.size(); i++) {
                    if(d->DiSubDir[i]->name == p[point]) {
                        d = d->DiSubDir[i];
                        point++;
                        isFind = true;
                        break;
                    }
                }
            }
        }
        if(!isFind) {
            //cout << "Wrong Path!" << endl;
            printRed("Wrong Path!\n");
            return;
        } else if(f != nullptr && point != p.size()) {
            //cout << "Wrong Path!" << endl;
            printRed("Wrong Path!\n");
            return;
        }
    }

    if(!isFind) {
        //cout << "Wrong Path!" << endl;
        printRed("Wrong Path!\n");
        return;
    }
    if(f != nullptr) {  // 路径解析的结果是文件
        if(type == 0) {
            //cout << f->path << endl;
            printWhite(f->path + "\n");
        } else if(type == 1) {
            //cout << f->path << " " << f->fileSize << endl;
            printWhite(f->path + " " + to_string(f->fileSize) + "\n");
        } else {
            catOut(f->cluster, f->fileSize);
        }
    } else {            // 路径解析的结果是目录
        if(type == 0) {
            lsOut(d);
        } else if(type == 1) {
            lslOut(d);
        } else {
            //cout << d->name << ": Is a directory!" << endl;
            printRed(d->name + ": Is a directory!\n");
        }
    }
}
// 解析命令
void parseCommand(const string& command) {
    if(command.length() == 0)
        return;
    vector<string> splitCom = split(command, " ");
    string path;
    bool isL = false;
    if(splitCom[0] == "ls") {
        if(splitCom.size() == 1) {
            path = "/";
            parsePath(path, 0);
        } else {
            for(int i = 1; i < splitCom.size(); i++) {
                string tmp = splitCom[i];
                if(tmp.at(0) == '-') {
                    if(tmp.length() == 1) {
                        //cout << "Wrong Option!" << endl;
                        printRed("Wrong Option!\n");
                        return;
                    }
                    for(int j = 1; j < tmp.length(); j++) {
                        if(tmp.at(j) != 'l') {
                            //cout << "Wrong Option!" << endl;
                            printRed("Wrong Option!\n");
                            return;
                        }
                    }
                    isL = true;
                }
            }

            int pathNum = 0;
            for(int i = 1; i < splitCom.size(); i++) {
                if(splitCom[i].at(0) != '-') {
                    pathNum++;
                    path = splitCom[i];
                }
            }
            if(pathNum > 1) {
                //cout << "Too Many Paths!" << endl;
                printRed("Too Many Paths!\n");
                return;
            }
            if(isL) {
                if(pathNum == 0) {
                    path = "/";
                    parsePath(path, 1);
                } else {
                    if(path.at(0) != '/') {
                        path = "/" + path;
                    }
                    parsePath(path, 1);
                }
            } else {
                if(path.at(0) != '/') {
                    path = "/" + path;
                }
                parsePath(path, 0);
            }
        }
    } else if(splitCom[0] == "cat") {
        if(splitCom.size() >= 3) {
            //cout << "Too Many Parameters!" << endl;
            printRed("Too Many Parameters!\n");
            return;
        } else if(splitCom.size() == 1) {
            //cout << "Need One Parameter!" << endl;
            printRed("Need One Parameter!\n");
            return;
        } else {
            parsePath(splitCom[1], 2);
        }
    } else {
        //cout << "Wrong Command!" << endl;
        printRed("Wrong Command!\n");
    }
}
// 删除节点，释放内存
void Delete(DNode* dNode) {
    for(int i = 0; i < dNode->DiSubFile.size(); i++) {
        delete dNode->DiSubFile[i];
    }
    for(int i = 0; i < dNode->DiSubDir.size(); i++) {
        Delete(dNode->DiSubDir[i]);
    }
    delete dNode;
}

int main() {
    // 打开文件
    in.open("./a.img", ios::in|ios::binary);
    if(!in.is_open()) {
        //cout << "File Not Found!" << endl;
        printRed("The ./a.img File Not Found!\n");
        return 0;
    }
    // 读取BPB
    in.seekg(11, ios::beg);
    in.read((char*) &bpb, sizeof(bpb));
    // 读取fat1
    fat = new char[bpb.BPB_FATSz16 * bpb.BPB_BytesPerSector];
    in.seekg(bpb.BPB_BytesPerSector * bpb.BPB_RsvSecCnt, ios::beg);
    in.read(fat, bpb.BPB_FATSz16 * bpb.BPB_BytesPerSector);
    // 读取所有根目录项
    rootEntries = vector<Entry>(bpb.BPB_RootEntCnt);
    in.seekg((bpb.BPB_RsvSecCnt + bpb.BPB_NumFATs * bpb.BPB_FATSz16) * bpb.BPB_BytesPerSector, ios::beg);
    for(int i = 0; i < bpb.BPB_RootEntCnt; i++) {
        in.read((char*) &rootEntries[i], sizeof(Entry));
    }
    // 建立根节点
    root->path = "/";
    root->name = "/";
    root->father = root;
    // 加载文件及目录
    Load(*root, 0);

    string s;
    while(true) {
        //cout << ">";
        printWhite(">");
        getline(cin, s);
        if(s == "exit") {
            in.close();
            break;
        }
        parseCommand(s);
    }
    delete[] fat;
    Delete(root);

    return 0;
}