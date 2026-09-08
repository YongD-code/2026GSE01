#pragma once
#include <cstdint>
#include <cmath>
#include <map>
#include <set>
#include <vector>
#include <utility>

struct WorldPoint { double x,y; };
struct WorldObject { int kind; double x,y,w,d,h; };

// Version 1 generation: coordinate hashes, never visitation order, define a chunk.
class World {
public:
    static constexpr int ChunkSize=512;
    using Coordinate=std::int64_t;
    using Key=std::pair<Coordinate,Coordinate>;
    struct Chunk {
        Key key;
        int biome;
        std::vector<WorldObject> objects;
    };
    std::map<Key,Chunk> chunks;
    static Coordinate Index(double value) {return static_cast<Coordinate>(std::floor(value/ChunkSize));}
    static std::uint64_t Hash(Coordinate x,Coordinate y,std::uint64_t salt=0) {
        std::uint64_t n=static_cast<std::uint64_t>(x)*0x9e3779b97f4a7c15ULL;
        n^=static_cast<std::uint64_t>(y)*0xbf58476d1ce4e5b9ULL;
        n^=2026ULL+salt*0x94d049bb133111ebULL;
        n=(n^(n>>30))*0xbf58476d1ce4e5b9ULL;
        n=(n^(n>>27))*0x94d049bb133111ebULL;
        return n^(n>>31);
    }
    static bool Road(Coordinate tileX,Coordinate tileY) {
        auto lane=[](Coordinate v) {Coordinate m=(v%8+8)%8;return m==0||m==7;};
        return lane(tileX)||lane(tileY);
    }
    static bool Village(double x,double y) {return std::abs(x)<512&&std::abs(y)<512;}
    Chunk& Ensure(Coordinate x,Coordinate y) {
        Key key={x,y};auto found=chunks.find(key);
        if(found!=chunks.end()) return found->second;
        Chunk c; c.key=key;
        // Regions cover four chunks, including correctly grouped negative coordinates.
        Coordinate rx=static_cast<Coordinate>(std::floor(double(x)/4));
        Coordinate ry=static_cast<Coordinate>(std::floor(double(y)/4));
        c.biome=static_cast<int>(Hash(rx,ry,9)%3);
        for(int row=0;row<3;++row) for(int col=0;col<3;++col) {
            auto h=Hash(x,y,1+row*3+col);
            if(h%5==0) continue;
            double px=double(x)*ChunkSize+85+col*112+double((h>>8)%24);
            double py=double(y)*ChunkSize+85+row*112+double((h>>16)%24);
            if(Village(px,py)) continue;
            int kind=c.biome==2?2:1;
            double w=kind==1?18:30,d=w,height=kind==1?75+double(h%55):20+double(h%35);
            // Rare roadside cottages; bounded footprint cannot block connecting roads.
            if(col==1&&row==1&&h%11==0) {kind=0;w=75;d=65;height=70;}
            c.objects.push_back({kind,px,py,w,d,height});
        }
        return chunks.emplace(key,std::move(c)).first->second;
    }
    void Stream(WorldPoint camera,WorldPoint actor,int width,int height) {
        // Inverse projection of screen bounds plus roof-height and movement margins.
        double reach=width/3.4+height/1.72+260;
        Coordinate minX=Index(camera.x-reach),maxX=Index(camera.x+reach);
        Coordinate minY=Index(camera.y-reach),maxY=Index(camera.y+reach);
        std::set<Key> needed;
        for(Coordinate y=minY;y<=maxY;++y) for(Coordinate x=minX;x<=maxX;++x) needed.insert({x,y});
        Coordinate ax=Index(actor.x),ay=Index(actor.y);
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x) needed.insert({ax+x,ay+y});
        for(const Key& key:needed) Ensure(key.first,key.second);
        for(auto it=chunks.begin();it!=chunks.end();) {
            if(needed.find(it->first)==needed.end()) it=chunks.erase(it);else ++it;
        }
    }
    bool Blocked(WorldPoint p) {
        Coordinate cx=Index(p.x),cy=Index(p.y);
        // Include neighbors so object footprints remain solid at chunk boundaries.
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
            for(const WorldObject& o:Ensure(cx+x,cy+y).objects)
                if(p.x>o.x-12&&p.x<o.x+o.w+12&&p.y>o.y-12&&p.y<o.y+o.d+12) return true;
        return false;
    }
    const char* Region(WorldPoint p) {
        if(Village(p.x,p.y)) return "잿빛 마을";
        switch(Ensure(Index(p.x),Index(p.y)).biome) {
        case 0:return "메마른 숲";case 1:return "안개 황야";default:return "잊힌 폐허";
        }
    }
};
